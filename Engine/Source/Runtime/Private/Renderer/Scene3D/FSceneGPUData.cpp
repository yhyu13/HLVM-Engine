#include "Renderer/Scene3D/FSceneGPUData.h"
#include "Renderer/Scene3D/Scene3DLoader.h"
#include "Renderer/Scene3D/Scene3DNode.h"
#include "Renderer/Mesh/StaticMesh.h"
#include "Renderer/Mesh/MeshCache.h"
#include "Renderer/Material/PBRMaterial.h"
#include "Renderer/RHI/Object/Buffer.h"
#include "Renderer/Texture/AsyncTextureLoader.h"
#include "Renderer/Texture/TextureCache.h"
#include "Core/Parallel/Async/WorkStealThreadPool.h"
#include "Core/Log.h"

#include <chrono>

DECLARE_LOG_CATEGORY(LogRenderer)

using FHighResClock = std::chrono::high_resolution_clock;

bool FSceneGPUData::Initialize(nvrhi::IDevice* InDevice, const FPath& ScenePath, FMeshCache* InMeshCache)
{
    if (bIsInitialized)
    {
        Shutdown();
    }

    const auto InitStartTime = FHighResClock::now();

    Device = InDevice;
    MeshCache = InMeshCache;
    CachedScenePath = ScenePath;

    // =====================================================================
    // Load scene
    // =====================================================================
    const auto SceneLoadStart = FHighResClock::now();
    HLVM_LOG(LogRenderer, info, TXT("FSceneGPUData: Loading scene from {}"), *ScenePath);
    Scene = FScene3DLoader::LoadFromFile(ScenePath);
    if (!Scene || Scene->IsEmpty())
    {
        HLVM_LOG(LogRenderer, err, TXT("FSceneGPUData: Failed to load scene"));
        return false;
    }
    const auto SceneLoadEnd = FHighResClock::now();
    const float SceneLoadMs = std::chrono::duration<float, std::milli>(SceneLoadEnd - SceneLoadStart).count();

    auto StaticMeshes = Scene->GetAllStaticMesh();
    auto Materials = Scene->GetAllMaterial();
    HLVM_LOG(LogRenderer, info, TXT("FSceneGPUData: Loaded scene with {} meshes, {} materials ({} ms)"),
        StaticMeshes.size(), Materials.size(), SceneLoadMs);

    // =====================================================================
    // Calculate bounding box
    // =====================================================================
    const auto BoundsStart = FHighResClock::now();
    CachedBBoxMin = glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX);
    CachedBBoxMax = glm::vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (const auto& Mesh : StaticMeshes)
    {
        for (auto& vert : Mesh->GetVertices())
        {
            glm::vec3 pos(vert.Position.x, vert.Position.y, vert.Position.z);
            CachedBBoxMin = glm::min(CachedBBoxMin, pos);
            CachedBBoxMax = glm::max(CachedBBoxMax, pos);
        }
    }
    CachedSceneCenter = (CachedBBoxMin + CachedBBoxMax) * 0.5f;
    CachedSceneRadius = glm::length(CachedBBoxMax - CachedBBoxMin) * 0.5f;
    const auto BoundsEnd = FHighResClock::now();
    const float BoundsMs = std::chrono::duration<float, std::milli>(BoundsEnd - BoundsStart).count();
    HLVM_LOG(LogRenderer, info, TXT("FSceneGPUData: Scene center: ({:.2f}, {:.2f}, {:.2f}), radius: {:.2f} ({} ms)"),
        CachedSceneCenter.x, CachedSceneCenter.y, CachedSceneCenter.z, CachedSceneRadius, BoundsMs);

    // =====================================================================
    // Create placeholder textures (1x1 white albedo + flat normal)
    // These are used while real textures are loading asynchronously.
    // =====================================================================
    const auto PlaceholderStart = FHighResClock::now();
    {
        nvrhi::TextureDesc Desc;
        Desc.dimension = nvrhi::TextureDimension::Texture2D;
        Desc.width = 1;
        Desc.height = 1;
        Desc.format = nvrhi::Format::RGBA8_UNORM;
        Desc.isRenderTarget = false;
        Desc.isUAV = false;
        Desc.isTypeless = false;
        Desc.initialState = nvrhi::ResourceStates::ShaderResource;
        Desc.keepInitialState = true;
        Desc.debugName = "PlaceholderTexture";
        PlaceholderTexture = Device->createTexture(Desc);

        nvrhi::CommandListHandle TexCmdList = Device->createCommandList();
        TexCmdList->open();
        uint32_t whitePixel = 0xFFFFFFFF;
        TexCmdList->writeTexture(PlaceholderTexture, 0, 0, &whitePixel, 4);

        Desc.debugName = "NormalPlaceholderTexture";
        NormalPlaceholderTexture = Device->createTexture(Desc);
        uint32_t flatNormalPixel = 0xFF8080FF; // ABGR: A=255, B=128, G=128, R=255
        TexCmdList->writeTexture(NormalPlaceholderTexture, 0, 0, &flatNormalPixel, 4);

        TexCmdList->close();
        Device->executeCommandList(TexCmdList);
    }

    // =====================================================================
    // Begin async texture loading (non-blocking)
    // =====================================================================
    HLVM_LOG(LogRenderer, info, TXT("FSceneGPUData: Starting async PBR texture loading..."));

    FAsyncTextureLoader::BeginAsyncLoad(
        Device, Materials,
        {IMaterial::ETextureType::Albedo, IMaterial::ETextureType::Normal,
         IMaterial::ETextureType::Metallic, IMaterial::ETextureType::Roughness,
         IMaterial::ETextureType::AmbientOcclusion});
    const auto AsyncLoadStartEnd = FHighResClock::now();
    const float AsyncLoadStartMs = std::chrono::duration<float, std::milli>(AsyncLoadStartEnd - PlaceholderStart).count();
    HLVM_LOG(LogRenderer, info, TXT("FSceneGPUData: Placeholder + async load enqueue took {} ms"), AsyncLoadStartMs);

    // =====================================================================
    // Build geometry deduplication and instance data from scene
    // =====================================================================
    const auto GeomStart = FHighResClock::now();
    HLVM_LOG(LogRenderer, info, TXT("FSceneGPUData: Building geometry and instance data from {} mesh occurrences..."), Scene->NumMeshes());

    // Clear previous data
    Geometries.clear();
    Instances.clear();

    // Map from (mesh pointer) to geometry index - for deduplication
    // Note: We deduplicate by mesh pointer identity since Assimp shares aiMesh objects
    std::unordered_map<std::shared_ptr<FStaticMesh>, uint32_t> MeshToGeometryIndex;

    // Process each mesh occurrence in the scene
    for (const auto& [Level, Mesh] : *Scene)
    {
        std::shared_ptr<FStaticMesh> StaticMesh = std::dynamic_pointer_cast<FStaticMesh>(Mesh);
        if (!StaticMesh)
        {
            continue;
        }

        // Get or create deduplicated geometry entry
        uint32_t GeometryIndex;
        auto it = MeshToGeometryIndex.find(StaticMesh);
        if (it != MeshToGeometryIndex.end())
        {
            // Reuse existing geometry
            GeometryIndex = it->second;
        }
        else
        {
            // Create new geometry entry
            FMeshGeometry Geometry;
            Geometry.Mesh = StaticMesh;
            Geometry.IndexCount = static_cast<uint32_t>(StaticMesh->GetIndices().size());

            // Check mesh cache first
            bool bCacheHit = false;
            if (MeshCache)
            {
                // Find mesh index in static meshes list for cache key
                uint32_t MeshIndex = 0;
                for (size_t i = 0; i < StaticMeshes.size(); ++i)
                {
                    if (StaticMeshes[i] == StaticMesh)
                    {
                        MeshIndex = static_cast<uint32_t>(i);
                        break;
                    }
                }

                FMeshCache::FMeshKey Key{CachedScenePath, MeshIndex};
                if (const FMeshCache::FMeshEntry* Entry = MeshCache->Find(Key))
                {
                    Geometry.VertexBuffer = Entry->VertexBuffer;
                    Geometry.IndexBuffer = Entry->IndexBuffer;
                    bCacheHit = true;
                    HLVM_LOG(LogRenderer, info, TXT("FSceneGPUData: Geometry[{}]: CACHE HIT ({} vertices, {} indices)"),
                        Geometries.size(), Entry->VertexCount, Entry->IndexCount);
                }
            }

            if (!bCacheHit)
            {
                const auto& Vertices = StaticMesh->GetVertices();
                const auto& Indices = StaticMesh->GetIndices();

                // Create vertex buffer
                nvrhi::BufferDesc VBDesc;
                VBDesc.byteSize = Vertices.size() * sizeof(FVertex);
                VBDesc.isVertexBuffer = true;
                VBDesc.isVolatile = false;
                VBDesc.initialState = nvrhi::ResourceStates::CopyDest;
                VBDesc.keepInitialState = true;
                VBDesc.debugName = "MeshVertexBuffer";
                Geometry.VertexBuffer = Device->createBuffer(VBDesc);

                // Create index buffer
                nvrhi::BufferDesc IBDesc;
                IBDesc.byteSize = Indices.size() * sizeof(uint32_t);
                IBDesc.isIndexBuffer = true;
                IBDesc.isVolatile = false;
                IBDesc.initialState = nvrhi::ResourceStates::CopyDest;
                IBDesc.keepInitialState = true;
                IBDesc.debugName = "MeshIndexBuffer";
                Geometry.IndexBuffer = Device->createBuffer(IBDesc);

                // Insert into cache
                if (MeshCache)
                {
                    uint32_t MeshIndex = 0;
                    for (size_t i = 0; i < StaticMeshes.size(); ++i)
                    {
                        if (StaticMeshes[i] == StaticMesh)
                        {
                            MeshIndex = static_cast<uint32_t>(i);
                            break;
                        }
                    }

                    FMeshCache::FMeshKey Key{CachedScenePath, MeshIndex};
                    FMeshCache::FMeshEntry Entry;
                    Entry.VertexBuffer = Geometry.VertexBuffer;
                    Entry.IndexBuffer = Geometry.IndexBuffer;
                    Entry.IndexCount = Geometry.IndexCount;
                    Entry.VertexCount = static_cast<uint32_t>(Vertices.size());
                    MeshCache->Insert(Key, Entry);
                }

                HLVM_LOG(LogRenderer, info, TXT("FSceneGPUData: Geometry[{}]: {} vertices, {} indices"),
                    Geometries.size(), Vertices.size(), Indices.size());
            }

            GeometryIndex = static_cast<uint32_t>(Geometries.size());
            Geometries.push_back(Geometry);
            MeshToGeometryIndex[StaticMesh] = GeometryIndex;
        }

        // Create instance entry with transform from this mesh occurrence
        FMeshInstance Instance;
        Instance.GeometryIndex = GeometryIndex;
        Instance.Transform = StaticMesh->WorldTransform;  // Transform stored by loader
        Instance.Mesh = StaticMesh;
        Instances.push_back(Instance);
    }

    const auto GeomEnd = FHighResClock::now();
    const float GeomMs = std::chrono::duration<float, std::milli>(GeomEnd - GeomStart).count();

    HLVM_LOG(LogRenderer, info, TXT("FSceneGPUData: {} unique geometries, {} total instances ({} ms)"),
        Geometries.size(), Instances.size(), GeomMs);

    const auto InitEndTime = FHighResClock::now();
    const float TotalInitMs = std::chrono::duration<float, std::milli>(InitEndTime - InitStartTime).count();

    HLVM_LOG(LogRenderer, info, TXT("FSceneGPUData: Initialized successfully (total: {} ms)"), TotalInitMs);
    bIsInitialized = true;
    return true;
}

void FSceneGPUData::Shutdown()
{
    Geometries.clear();
    Instances.clear();
    Scene.reset();

    PlaceholderTexture = nullptr;
    NormalPlaceholderTexture = nullptr;

    Device = nullptr;
    CachedSceneCenter = glm::vec3(0.f);
    CachedBBoxMin = glm::vec3(0.f);
    CachedBBoxMax = glm::vec3(0.f);
    bIsInitialized = false;
}

FSceneGPUData::FDrawData FSceneGPUData::BuildDrawData()
{
    FDrawData Result;
    Result.SceneCenter = CachedSceneCenter;
    Result.BBoxMin = CachedBBoxMin;
    Result.BBoxMax = CachedBBoxMax;
    Result.SceneRadius = CachedSceneRadius;

    Result.ShadowItems.reserve(Instances.size());
    Result.GBufferItems.reserve(Instances.size());

    for (const auto& Instance : Instances)
    {
        const auto& Geometry = Geometries[Instance.GeometryIndex];

        // Shadow item
        FShadowMapPass::FMeshDrawItem ShadowItem;
        ShadowItem.VertexBuffer = Geometry.VertexBuffer;
        ShadowItem.IndexBuffer = Geometry.IndexBuffer;
        ShadowItem.IndexCount = Geometry.IndexCount;
        Result.ShadowItems.push_back(ShadowItem);

        // GBuffer item with material lookup
        FDeferredFrameRenderer::FGBufferMeshItem GBufferItem;
        GBufferItem.VertexBuffer = Geometry.VertexBuffer;
        GBufferItem.IndexBuffer = Geometry.IndexBuffer;
        GBufferItem.IndexCount = Geometry.IndexCount;
        GBufferItem.ModelMatrix = Instance.Transform;  // Per-mesh world transform

        nvrhi::TextureHandle DiffuseTex = PlaceholderTexture;
        nvrhi::TextureHandle NormalTex = NormalPlaceholderTexture;
        nvrhi::TextureHandle MetallicTex = PlaceholderTexture;
        nvrhi::TextureHandle RoughnessTex = PlaceholderTexture;
        nvrhi::TextureHandle AOTex = PlaceholderTexture;
        FGBufferFillPass::FMaterialConstants MatConst;
        memset(&MatConst, 0, sizeof(MatConst));
        MatConst.AlbedoTint[0] = 1.0f;
        MatConst.AlbedoTint[1] = 1.0f;
        MatConst.AlbedoTint[2] = 1.0f;
        MatConst.AlbedoTint[3] = 1.0f;
        MatConst.Metallic = 0.0f;
        MatConst.Roughness = 1.0f;

        if (Scene && Instance.Mesh)
        {
            auto it = Scene->MeshMultiMaterialMap.find(Instance.Mesh);
            if (it != Scene->MeshMultiMaterialMap.end() && !it->second.empty())
            {
                if (auto PBRMat = std::dynamic_pointer_cast<FPBRMaterial>(it->second[0]))
                {
                    if (PBRMat->HasGPUTexture(IMaterial::ETextureType::Albedo))
                        DiffuseTex = PBRMat->GetGPUTexture(IMaterial::ETextureType::Albedo).GetTextureSRV();
                    if (PBRMat->HasGPUTexture(IMaterial::ETextureType::Normal))
                        NormalTex = PBRMat->GetGPUTexture(IMaterial::ETextureType::Normal).GetTextureSRV();
                    if (PBRMat->HasGPUTexture(IMaterial::ETextureType::Metallic))
                        MetallicTex = PBRMat->GetGPUTexture(IMaterial::ETextureType::Metallic).GetTextureSRV();
                    if (PBRMat->HasGPUTexture(IMaterial::ETextureType::Roughness))
                        RoughnessTex = PBRMat->GetGPUTexture(IMaterial::ETextureType::Roughness).GetTextureSRV();
                    if (PBRMat->HasGPUTexture(IMaterial::ETextureType::AmbientOcclusion))
                        AOTex = PBRMat->GetGPUTexture(IMaterial::ETextureType::AmbientOcclusion).GetTextureSRV();
                    FVec3 albedo = PBRMat->GetAlbedoColor();
                    MatConst.AlbedoTint[0] = albedo.x;
                    MatConst.AlbedoTint[1] = albedo.y;
                    MatConst.AlbedoTint[2] = albedo.z;
                    MatConst.Metallic = PBRMat->GetMetallic();
                    MatConst.Roughness = PBRMat->GetRoughness();
                }
            }
        }

        GBufferItem.DiffuseTexture = DiffuseTex;
        GBufferItem.NormalTexture = NormalTex;
        GBufferItem.MetallicTexture = MetallicTex;
        GBufferItem.RoughnessTexture = RoughnessTex;
        GBufferItem.AOTexture = AOTex;
        GBufferItem.MaterialConstants = MatConst;
        Result.GBufferItems.push_back(GBufferItem);
    }

    return Result;
}
