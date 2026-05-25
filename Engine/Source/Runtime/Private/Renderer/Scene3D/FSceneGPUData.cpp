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
    // Create geometry buffers (batched per-mesh)
    // =====================================================================
    const auto GeomStart = FHighResClock::now();
    HLVM_LOG(LogRenderer, info, TXT("FSceneGPUData: Creating geometry buffers for {} meshes..."), StaticMeshes.size());

    nvrhi::CommandListHandle GeomCmdList = Device->createCommandList();
    GeomCmdList->open();

    MeshGPUData.reserve(StaticMeshes.size());
    for (size_t i = 0; i < StaticMeshes.size(); ++i)
    {
        const auto& Mesh = StaticMeshes[i];
        const auto& Vertices = Mesh->GetVertices();
        const auto& Indices = Mesh->GetIndices();

        FMeshGPUData DrawData;
        DrawData.Mesh = Mesh;
        DrawData.IndexCount = static_cast<uint32_t>(Indices.size());

        // Check mesh cache first
        bool bCacheHit = false;
        if (MeshCache)
        {
            FMeshCache::FMeshKey Key{CachedScenePath, static_cast<uint32_t>(i)};
            if (const FMeshCache::FMeshEntry* Entry = MeshCache->Find(Key))
            {
                DrawData.VertexBuffer = Entry->VertexBuffer;
                DrawData.IndexBuffer = Entry->IndexBuffer;
                DrawData.IndexCount = Entry->IndexCount;
                bCacheHit = true;
                HLVM_LOG(LogRenderer, info, TXT("FSceneGPUData: Mesh[{}]: CACHE HIT ({} vertices, {} indices)"),
                    i, Entry->VertexCount, Entry->IndexCount);
            }
        }

        if (!bCacheHit)
        {
            // Create vertex buffer
            {
                nvrhi::BufferDesc VBDesc;
                VBDesc.byteSize = Vertices.size() * sizeof(FVertex);
                VBDesc.isVertexBuffer = true;
                VBDesc.isVolatile = false;
                VBDesc.initialState = nvrhi::ResourceStates::CopyDest;
                VBDesc.keepInitialState = true;
                VBDesc.debugName = "MeshVertexBuffer";
                DrawData.VertexBuffer = Device->createBuffer(VBDesc);

                GeomCmdList->beginTrackingBufferState(DrawData.VertexBuffer, nvrhi::ResourceStates::CopyDest);
                GeomCmdList->writeBuffer(DrawData.VertexBuffer, Vertices.data(), VBDesc.byteSize);
                GeomCmdList->setPermanentBufferState(DrawData.VertexBuffer, nvrhi::ResourceStates::VertexBuffer);
            }

            // Create index buffer
            {
                nvrhi::BufferDesc IBDesc;
                IBDesc.byteSize = Indices.size() * sizeof(uint32_t);
                IBDesc.isIndexBuffer = true;
                IBDesc.isVolatile = false;
                IBDesc.initialState = nvrhi::ResourceStates::CopyDest;
                IBDesc.keepInitialState = true;
                IBDesc.debugName = "MeshIndexBuffer";
                DrawData.IndexBuffer = Device->createBuffer(IBDesc);

                GeomCmdList->beginTrackingBufferState(DrawData.IndexBuffer, nvrhi::ResourceStates::CopyDest);
                GeomCmdList->writeBuffer(DrawData.IndexBuffer, Indices.data(), IBDesc.byteSize);
                GeomCmdList->setPermanentBufferState(DrawData.IndexBuffer, nvrhi::ResourceStates::IndexBuffer);
            }

            // Insert into cache
            if (MeshCache)
            {
                FMeshCache::FMeshKey Key{CachedScenePath, static_cast<uint32_t>(i)};
                FMeshCache::FMeshEntry Entry;
                Entry.VertexBuffer = DrawData.VertexBuffer;
                Entry.IndexBuffer = DrawData.IndexBuffer;
                Entry.IndexCount = DrawData.IndexCount;
                Entry.VertexCount = static_cast<uint32_t>(Vertices.size());
                MeshCache->Insert(Key, Entry);
            }

            HLVM_LOG(LogRenderer, info, TXT("FSceneGPUData: Mesh[{}]: {} vertices, {} indices"),
                i, Vertices.size(), Indices.size());
        }

        MeshGPUData.push_back(DrawData);
    }

    GeomCmdList->close();
    Device->executeCommandList(GeomCmdList);
    Device->waitForIdle();
    const auto GeomEnd = FHighResClock::now();
    const float GeomMs = std::chrono::duration<float, std::milli>(GeomEnd - GeomStart).count();

    const auto InitEndTime = FHighResClock::now();
    const float TotalInitMs = std::chrono::duration<float, std::milli>(InitEndTime - InitStartTime).count();

    HLVM_LOG(LogRenderer, info, TXT("FSceneGPUData: Geometry buffers created ({} ms)"), GeomMs);
    HLVM_LOG(LogRenderer, info, TXT("FSceneGPUData: Initialized successfully (total: {} ms)"), TotalInitMs);
    bIsInitialized = true;
    return true;
}

void FSceneGPUData::Shutdown()
{
    MeshGPUData.clear();
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

    Result.ShadowItems.reserve(MeshGPUData.size());
    Result.GBufferItems.reserve(MeshGPUData.size());

    for (const auto& MeshData : MeshGPUData)
    {
        // Shadow item
        FShadowMapPass::FMeshDrawItem ShadowItem;
        ShadowItem.VertexBuffer = MeshData.VertexBuffer;
        ShadowItem.IndexBuffer = MeshData.IndexBuffer;
        ShadowItem.IndexCount = MeshData.IndexCount;
        Result.ShadowItems.push_back(ShadowItem);

        // GBuffer item with material lookup
        FDeferredFrameRenderer::FGBufferMeshItem GBufferItem;
        GBufferItem.VertexBuffer = MeshData.VertexBuffer;
        GBufferItem.IndexBuffer = MeshData.IndexBuffer;
        GBufferItem.IndexCount = MeshData.IndexCount;

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

        if (MeshData.Mesh && Scene)
        {
            auto it = Scene->MeshMultiMaterialMap.find(MeshData.Mesh);
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
