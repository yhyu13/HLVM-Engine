#include "Renderer/Scene3D/FSceneGPUData.h"
#include "Renderer/Scene3D/Scene3DLoader.h"
#include "Renderer/Scene3D/Scene3DNode.h"
#include "Renderer/Mesh/StaticMesh.h"
#include "Renderer/Material/PBRMaterial.h"
#include "Renderer/RHI/Object/Buffer.h"
#include "Renderer/Texture/AsyncTextureLoader.h"
#include "Renderer/Texture/TextureCache.h"
#include "Core/Parallel/Async/WorkStealThreadPool.h"
#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogRenderer)

bool FSceneGPUData::Initialize(nvrhi::IDevice* InDevice, const FPath& ScenePath)
{
    if (bIsInitialized)
    {
        Shutdown();
    }

    Device = InDevice;

    // =====================================================================
    // Load scene
    // =====================================================================
    HLVM_LOG(LogRenderer, info, TXT("FSceneGPUData: Loading scene from {}"), *ScenePath);
    Scene = FScene3DLoader::LoadFromFile(ScenePath);
    if (!Scene || Scene->IsEmpty())
    {
        HLVM_LOG(LogRenderer, err, TXT("FSceneGPUData: Failed to load scene"));
        return false;
    }

    auto StaticMeshes = Scene->GetAllStaticMesh();
    auto Materials = Scene->GetAllMaterial();
    HLVM_LOG(LogRenderer, info, TXT("FSceneGPUData: Loaded scene with {} meshes, {} materials"),
        StaticMeshes.size(), Materials.size());

    // =====================================================================
    // Calculate bounding box
    // =====================================================================
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
    HLVM_LOG(LogRenderer, info, TXT("FSceneGPUData: Scene center: ({:.2f}, {:.2f}, {:.2f}), radius: {:.2f}"),
        CachedSceneCenter.x, CachedSceneCenter.y, CachedSceneCenter.z, CachedSceneRadius);

    // =====================================================================
    // Load PBR textures for materials (async decode + batched GPU upload)
    // =====================================================================
    HLVM_LOG(LogRenderer, info, TXT("FSceneGPUData: Loading PBR textures..."));

    FAsyncTextureLoader::LoadMaterialTexturesAsync(
        Device, Materials,
        {IMaterial::ETextureType::Albedo, IMaterial::ETextureType::Normal});

    // =====================================================================
    // Create placeholder textures (1x1 white albedo + flat normal)
    // =====================================================================
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
    // Create geometry buffers (batched per-mesh)
    // =====================================================================
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

        MeshGPUData.push_back(DrawData);

        HLVM_LOG(LogRenderer, info, TXT("FSceneGPUData: Mesh[{}]: {} vertices, {} indices"),
            i, Vertices.size(), Indices.size());
    }

    GeomCmdList->close();
    Device->executeCommandList(GeomCmdList);
    Device->waitForIdle();

    HLVM_LOG(LogRenderer, info, TXT("FSceneGPUData: Initialized successfully"));
    bIsInitialized = true;
    return true;
}

void FSceneGPUData::Shutdown()
{
    MeshGPUData.clear();
    Scene.reset();

    PlaceholderTexture = nullptr;
    NormalPlaceholderTexture = nullptr;

    // Clear texture cache to release NVRHI handles before device destruction
    FTextureCache::Get().Clear();

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
        FGBufferFillPass::FMaterialConstants MatConst;
        memset(&MatConst, 0, sizeof(MatConst));
        MatConst.AlbedoTint[0] = 1.0f;
        MatConst.AlbedoTint[1] = 1.0f;
        MatConst.AlbedoTint[2] = 1.0f;
        MatConst.AlbedoTint[3] = 1.0f;
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
        GBufferItem.MaterialConstants = MatConst;
        Result.GBufferItems.push_back(GBufferItem);
    }

    return Result;
}
