/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * RT Shadows GBuffer Test - Textured Sponza GBuffer + Ray Traced Shadows
 *
 * Pipeline:
 * 1. Load Sponza scene (27 meshes) with glTF textures
 * 2. GBuffer Pass: Render all meshes to 5 MRTs (Diffuse, Specular, Normal, Emissive, WorldPos)
 * 3. RT Shadow Pass: Read GBuffer world position, cast shadow rays toward animated light
 * 4. Blit Pass: Copy RT output to swapchain
 *
 * Scene is static; light direction rotates over time.
 */

#include "Test.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/RHI/Object/Buffer.h"
#include "Renderer/Common/FBindingCache.h"
#include "Renderer/Common/FCommonRenderPasses.h"
#include "Renderer/SceneGraph/FNode.h"
#include "Renderer/Scene3D/Scene3DLoader.h"
#include "Renderer/Mesh/StaticMesh.h"
#include "Renderer/Material/PBRMaterial.h"
#include "Renderer/Texture/AsyncTextureLoader.h"
#include "Renderer/RayTracing/BLASBuilder.h"
#include "Renderer/RayTracing/TLASBuilder.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
#include "Image/FImageDump.h"
#include "Core/Parallel/Async/WorkStealThreadPool.h"
#include <nvrhi/utils.h>
#include <Utility/Timer.h>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

DECLARE_LOG_CATEGORY(LogTest)

#if HLVM_VULKAN_RENDERER

// =============================================================================
// CONFIGURATION
// =============================================================================

static const char*    WINDOW_TITLE = "RT Shadows GBuffer Test";
static const uint32_t WINDOW_WIDTH = 800;
static const uint32_t WINDOW_HEIGHT = 600;

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static std::vector<char> ReadBinaryFile(const std::string& Filename)
{
    std::ifstream file(Filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file: " + Filename);
    }

    size_t            fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    file.close();

    return buffer;
}

// =============================================================================
// FRTShadowsGBufferPass
// =============================================================================

class FRTShadowsGBufferPass : public IRenderPass
{
public:
    using IRenderPass::IRenderPass;

    bool Initialize(nvrhi::IDevice* Device, nvrhi::IFramebuffer* Framebuffer, const FString& InWindowTitle)
    {
        HLVM_LOG(LogTest, info, TXT("=== FRTShadowsGBufferPass::Initialize ==="));

        NvrhiDevice = Device;
        FBInfo = Framebuffer->getFramebufferInfo();
        WindowTitle = InWindowTitle;

        BindingCache.SetDevice(NvrhiDevice);

        const auto DataDir = FString::Format(
            TXT("{}/Engine/Source/Runtime/Test/TestRTShadowsGBuffer_Data"),
            *GProjectRoot);

        // =====================================================================
        // Load RT shader from ShaderMake blob
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Loading RT shader..."));
        auto RTShaderBlob = ReadBinaryFile(
            FPath::Combine(DataDir, TXT("RTShadowsGBuffer.sblob")).string());

        const void* RTShaderBinary = nullptr;
        size_t      RTShaderBinarySize = 0;
        if (!ShaderMake::FindPermutationInBlob(RTShaderBlob.data(), RTShaderBlob.size(), nullptr, 0, &RTShaderBinary, &RTShaderBinarySize))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to extract RT shader from blob"));
            return false;
        }

        ShaderLibrary = NvrhiDevice->createShaderLibrary(
            RTShaderBinary, RTShaderBinarySize);
        if (!ShaderLibrary)
        {
            HLVM_LOG(LogTest, err, TXT("Failed to load RT shader library"));
            return false;
        }

        RayGenShader = ShaderLibrary->getShader("RayGen", nvrhi::ShaderType::RayGeneration);
        ClosestHitShader = ShaderLibrary->getShader("ClosestHit", nvrhi::ShaderType::ClosestHit);
        MissShader = ShaderLibrary->getShader("Miss", nvrhi::ShaderType::Miss);
        ShadowMissShader = ShaderLibrary->getShader("ShadowMiss", nvrhi::ShaderType::Miss);

        if (!RayGenShader || !ClosestHitShader || !MissShader || !ShadowMissShader)
        {
            HLVM_LOG(LogTest, err, TXT("Failed to get RT shaders"));
            return false;
        }
        HLVM_LOG(LogTest, info, TXT("RT shaders loaded successfully"));

        // =====================================================================
        // Load GBuffer shaders
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Loading GBuffer shaders..."));

        auto GBufferVSBlob = ReadBinaryFile(
            FPath::Combine(DataDir, TXT("GBufferSponzaVS.sblob")).string());
        const void* GBufferVSBinary = nullptr;
        size_t GBufferVSBinarySize = 0;
        if (!ShaderMake::FindPermutationInBlob(GBufferVSBlob.data(), GBufferVSBlob.size(), nullptr, 0, &GBufferVSBinary, &GBufferVSBinarySize))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to extract GBufferVS from blob"));
            return false;
        }

        auto GBufferPSBlob = ReadBinaryFile(
            FPath::Combine(DataDir, TXT("GBufferSponzaPS.sblob")).string());
        const void* GBufferPSBinary = nullptr;
        size_t GBufferPSBinarySize = 0;
        if (!ShaderMake::FindPermutationInBlob(GBufferPSBlob.data(), GBufferPSBlob.size(), nullptr, 0, &GBufferPSBinary, &GBufferPSBinarySize))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to extract GBufferPS from blob"));
            return false;
        }

        nvrhi::ShaderDesc VSDesc;
        VSDesc.setShaderType(nvrhi::ShaderType::Vertex);
        GBufferVS = NvrhiDevice->createShader(VSDesc, GBufferVSBinary, GBufferVSBinarySize);

        nvrhi::ShaderDesc PSDesc;
        PSDesc.setShaderType(nvrhi::ShaderType::Pixel);
        GBufferPS = NvrhiDevice->createShader(PSDesc, GBufferPSBinary, GBufferPSBinarySize);

        if (!GBufferVS || !GBufferPS)
        {
            HLVM_LOG(LogTest, err, TXT("Failed to create GBuffer shaders"));
            return false;
        }
        HLVM_LOG(LogTest, info, TXT("GBuffer shaders loaded successfully"));

        // =====================================================================
        // Create command list for initialization
        // =====================================================================
        nvrhi::CommandListHandle InitCmdList = NvrhiDevice->createCommandList();
        InitCmdList->open();

        // =====================================================================
        // Load Sponza scene
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Loading Sponza scene..."));
        const FString GitRoot = FString::Format(TXT("{}"), *GProjectRoot);
        const FPath   ScenePath = FPath(FString::Format(
              TXT("{}/Samples/Assets/Sponza/glTF/Sponza.gltf"), *GitRoot));

        Scene = FScene3DLoader::LoadFromFile(ScenePath);
        if (!Scene || Scene->IsEmpty())
        {
            HLVM_LOG(LogTest, err, TXT("Failed to load Sponza scene"));
            return false;
        }

        auto StaticMeshes = Scene->GetAllStaticMesh();
        auto Materials = Scene->GetAllMaterial();
        HLVM_LOG(LogTest, info, TXT("Loaded scene with {} meshes, {} materials"),
            StaticMeshes.size(), Materials.size());

        // Calculate scene bounding box and center
        glm::vec3 BBoxMin(FLT_MAX, FLT_MAX, FLT_MAX);
        glm::vec3 BBoxMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        for (const auto& Mesh : StaticMeshes)
        {
            for (auto& vert : Mesh->GetVertices())
            {
                glm::vec3 pos(vert.Position.x, vert.Position.y, vert.Position.z);
                BBoxMin = glm::min(BBoxMin, pos);
                BBoxMax = glm::max(BBoxMax, pos);
            }
        }
        HLVM_LOG(LogTest, info, TXT("Scene bounding box: Min({:.2f}, {:.2f}, {:.2f}), Max({:.2f}, {:.2f}, {:.2f})"),
            BBoxMin.x, BBoxMin.y, BBoxMin.z, BBoxMax.x, BBoxMax.y, BBoxMax.z);
        SceneCenter = (BBoxMin + BBoxMax) * 0.5f;
        float SceneRadius = glm::length(BBoxMax - BBoxMin) * 0.5f;
        HLVM_LOG(LogTest, info, TXT("Scene center: ({:.2f}, {:.2f}, {:.2f}), radius: {:.2f}"),
            SceneCenter.x, SceneCenter.y, SceneCenter.z, SceneRadius);

        // =====================================================================
        // Build BLAS for each mesh
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Building BLAS for {} meshes..."), StaticMeshes.size());
        for (const auto& Mesh : StaticMeshes)
        {
            nvrhi::rt::AccelStructHandle BLAS = BLASBuilder::Build(NvrhiDevice, InitCmdList.Get(), *Mesh);
            if (!BLAS)
            {
                HLVM_LOG(LogTest, err, TXT("Failed to build BLAS for mesh"));
                return false;
            }
            MeshBLASHandles.push_back(BLAS);
        }

        // =====================================================================
        // Build TLAS
        // =====================================================================
        if (!TLASBuilder.Initialize(NvrhiDevice, static_cast<uint32_t>(StaticMeshes.size())))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to initialize TLAS builder"));
            return false;
        }
        HLVM_LOG(LogTest, info, TXT("Building TLAS with {} instances..."), StaticMeshes.size());

        for (uint32_t i = 0; i < static_cast<uint32_t>(StaticMeshes.size()); ++i)
        {
            FTLASBuilder::FInstanceDesc InstanceDesc;
            InstanceDesc.BottomLevelAS = MeshBLASHandles[i];
            InstanceDesc.InstanceMask = 1;
            InstanceDesc.InstanceFlags = nvrhi::rt::InstanceFlags::TriangleFrontCounterclockwise;
            // Apply 2x scale to match TestSponzaDeferred
            glm::mat4 MeshTransform = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
            InstanceDesc.SetTransform(MeshTransform);
            if (!TLASBuilder.AddInstance(InstanceDesc))
            {
                HLVM_LOG(LogTest, err, TXT("Failed to add instance to TLAS"));
                return false;
            }
        }
        if (!TLASBuilder.Build(InitCmdList.Get()))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to build TLAS"));
            return false;
        }
        TopLevelAS = TLASBuilder.GetTLAS();
        HLVM_LOG(LogTest, info, TXT("TLAS built with {} instances"), TLASBuilder.GetInstanceCount());

        InitCmdList->close();
        NvrhiDevice->executeCommandList(InitCmdList);

        // =====================================================================
        // Load PBR textures for materials (async decode + batched GPU upload)
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Loading PBR textures..."));

        struct FPendingTexture {
            FPath TexturePath;
            FTexture* OutTexture;
            std::future<FDecodedImage> Future;
        };
        TVector<FPendingTexture> PendingTextures;

        nvrhi::CommandListHandle UploadCmdList = NvrhiDevice->createCommandList();
        UploadCmdList->open();

        for (auto& Mat : Materials)
        {
            auto PBRMat = std::dynamic_pointer_cast<FPBRMaterial>(Mat);
            if (!PBRMat)
                continue;

            if (PBRMat->HasTexture(IMaterial::ETextureType::Albedo) &&
                !PBRMat->HasGPUTexture(IMaterial::ETextureType::Albedo))
            {
                FPath TexturePath = PBRMat->GetTexturePath(IMaterial::ETextureType::Albedo);
                FString Ext = FPath::GetExtension(TexturePath);

                // KTX: not thread-safe and has ktx2/ fallback logic in LoadTexture.
                // Use the existing synchronous path on the main thread.
                // STB is thread-safe; enqueue to worker pool.
                if (Ext == ".ktx" || Ext == ".KTX" ||
                    Ext == ".ktx2" || Ext == ".KTX2")
                {
                    nvrhi::CommandListHandle KtxCmdList = NvrhiDevice->createCommandList();
                    KtxCmdList->open();
                    PBRMat->LoadTexture(IMaterial::ETextureType::Albedo, NvrhiDevice, KtxCmdList);
                }
                else
                {
                    FPendingTexture Pending;
                    Pending.TexturePath = TexturePath;
                    Pending.OutTexture = &PBRMat->GetGPUTexture(IMaterial::ETextureType::Albedo);
                    Pending.Future = FWorkStealThreadPool::Get()->EnqueueTask(
                        [TexturePath]() {
                            return FAsyncTextureLoader::DecodeSTBTexture(TexturePath);
                        });
                    PendingTextures.push_back(MoveTemp(Pending));
                }
            }
        }

        // Wait for async STB decode jobs and upload on the main thread
        for (auto& Pending : PendingTextures)
        {
            FDecodedImage Decoded = Pending.Future.get();
            if (Decoded.bIsValid)
            {
                FAsyncTextureLoader::UploadTextureToCommandList(
                    NvrhiDevice, UploadCmdList, Decoded, *Pending.OutTexture);
            }
            else
            {
                HLVM_LOG(LogTest, warn,
                    TXT("Failed to async-decode texture: {}"),
                    *FString(Pending.TexturePath.string().c_str()));
            }
        }

        UploadCmdList->close();
        NvrhiDevice->executeCommandList(UploadCmdList);
        NvrhiDevice->waitForIdle();

        // =====================================================================
        // Create GBuffer textures and framebuffer
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Creating GBuffer textures..."));

        uint32_t GBufferWidth = Framebuffer->getFramebufferInfo().width;
        uint32_t GBufferHeight = Framebuffer->getFramebufferInfo().height;

        // MRT0: Diffuse (RGBA8)
        {
            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = GBufferWidth;
            Desc.height = GBufferHeight;
            Desc.format = nvrhi::Format::RGBA8_UNORM;
            Desc.isRenderTarget = true;
            Desc.isUAV = false;
            Desc.initialState = nvrhi::ResourceStates::RenderTarget;
            Desc.keepInitialState = true;
            Desc.debugName = "GBufferDiffuse";
            GBufferDiffuseTexture = NvrhiDevice->createTexture(Desc);
        }

        // MRT1: Specular (RGBA16F)
        {
            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = GBufferWidth;
            Desc.height = GBufferHeight;
            Desc.format = nvrhi::Format::RGBA16_FLOAT;
            Desc.isRenderTarget = true;
            Desc.isUAV = false;
            Desc.initialState = nvrhi::ResourceStates::RenderTarget;
            Desc.keepInitialState = true;
            Desc.debugName = "GBufferSpecular";
            GBufferSpecularTexture = NvrhiDevice->createTexture(Desc);
        }

        // MRT2: Normals (RGBA16F)
        {
            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = GBufferWidth;
            Desc.height = GBufferHeight;
            Desc.format = nvrhi::Format::RGBA16_FLOAT;
            Desc.isRenderTarget = true;
            Desc.isUAV = false;
            Desc.initialState = nvrhi::ResourceStates::RenderTarget;
            Desc.keepInitialState = true;
            Desc.debugName = "GBufferNormals";
            GBufferNormalsTexture = NvrhiDevice->createTexture(Desc);
        }

        // MRT3: Emissive (RGBA16F)
        {
            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = GBufferWidth;
            Desc.height = GBufferHeight;
            Desc.format = nvrhi::Format::RGBA16_FLOAT;
            Desc.isRenderTarget = true;
            Desc.isUAV = false;
            Desc.initialState = nvrhi::ResourceStates::RenderTarget;
            Desc.keepInitialState = true;
            Desc.debugName = "GBufferEmissive";
            GBufferEmissiveTexture = NvrhiDevice->createTexture(Desc);
        }

        // MRT4: WorldPos (RGBA16F) - for RT shadow reconstruction
        {
            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = GBufferWidth;
            Desc.height = GBufferHeight;
            Desc.format = nvrhi::Format::RGBA16_FLOAT;
            Desc.isRenderTarget = true;
            Desc.isUAV = false;
            Desc.initialState = nvrhi::ResourceStates::RenderTarget;
            Desc.keepInitialState = true;
            Desc.debugName = "GBufferWorldPos";
            GBufferWorldPosTexture = NvrhiDevice->createTexture(Desc);
        }

        // Depth texture (D32)
        {
            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = GBufferWidth;
            Desc.height = GBufferHeight;
            Desc.format = nvrhi::Format::D32;
            Desc.isRenderTarget = true;
            Desc.isUAV = false;
            Desc.isTypeless = true;
            Desc.initialState = nvrhi::ResourceStates::DepthWrite;
            Desc.keepInitialState = true;
            Desc.debugName = "GBufferDepth";
            GBufferDepthTexture = NvrhiDevice->createTexture(Desc);
        }

        // Create GBuffer framebuffer
        {
            nvrhi::FramebufferDesc FBDesc;
            nvrhi::FramebufferAttachment DiffuseAttach;
            DiffuseAttach.setTexture(GBufferDiffuseTexture);
            FBDesc.addColorAttachment(DiffuseAttach);

            nvrhi::FramebufferAttachment SpecularAttach;
            SpecularAttach.setTexture(GBufferSpecularTexture);
            FBDesc.addColorAttachment(SpecularAttach);

            nvrhi::FramebufferAttachment NormalsAttach;
            NormalsAttach.setTexture(GBufferNormalsTexture);
            FBDesc.addColorAttachment(NormalsAttach);

            nvrhi::FramebufferAttachment EmissiveAttach;
            EmissiveAttach.setTexture(GBufferEmissiveTexture);
            FBDesc.addColorAttachment(EmissiveAttach);

            nvrhi::FramebufferAttachment WorldPosAttach;
            WorldPosAttach.setTexture(GBufferWorldPosTexture);
            FBDesc.addColorAttachment(WorldPosAttach);

            nvrhi::FramebufferAttachment DepthAttach;
            DepthAttach.setTexture(GBufferDepthTexture);
            FBDesc.setDepthAttachment(DepthAttach);

            GBufferFramebuffer = NvrhiDevice->createFramebuffer(FBDesc);
            if (!GBufferFramebuffer)
            {
                HLVM_LOG(LogTest, err, TXT("Failed to create GBuffer framebuffer"));
                return false;
            }
        }
        HLVM_LOG(LogTest, info, TXT("GBuffer framebuffer created with 5 color attachments"));

        // =====================================================================
        // Create per-mesh geometry buffers
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Creating geometry buffers for {} meshes..."), StaticMeshes.size());

        AllMeshDrawData.reserve(StaticMeshes.size());

        for (size_t i = 0; i < StaticMeshes.size(); ++i)
        {
            const auto& Mesh = StaticMeshes[i];
            const auto& Vertices = Mesh->GetVertices();
            const auto& Indices = Mesh->GetIndices();

            FMeshDrawData DrawData;
            DrawData.Mesh = Mesh;
            DrawData.IndexCount = static_cast<uint32_t>(Indices.size());

            // Create vertex buffer
            {
                nvrhi::BufferDesc VBDesc;
                VBDesc.byteSize = Vertices.size() * sizeof(FVertex);
                VBDesc.isVertexBuffer = true;
                VBDesc.isVolatile = false;
                VBDesc.initialState = nvrhi::ResourceStates::CopyDest;
                VBDesc.debugName = "MeshVertexBuffer";
                DrawData.VertexBuffer = NvrhiDevice->createBuffer(VBDesc);

                nvrhi::CommandListHandle GeomCmdList = NvrhiDevice->createCommandList();
                GeomCmdList->open();
                GeomCmdList->beginTrackingBufferState(DrawData.VertexBuffer, nvrhi::ResourceStates::CopyDest);
                GeomCmdList->writeBuffer(DrawData.VertexBuffer, Vertices.data(), VBDesc.byteSize);
                GeomCmdList->setPermanentBufferState(DrawData.VertexBuffer, nvrhi::ResourceStates::VertexBuffer);
                GeomCmdList->close();
                NvrhiDevice->executeCommandList(GeomCmdList);
            }

            // Create index buffer
            {
                nvrhi::BufferDesc IBDesc;
                IBDesc.byteSize = Indices.size() * sizeof(uint32_t);
                IBDesc.isIndexBuffer = true;
                IBDesc.isVolatile = false;
                IBDesc.initialState = nvrhi::ResourceStates::CopyDest;
                IBDesc.debugName = "MeshIndexBuffer";
                DrawData.IndexBuffer = NvrhiDevice->createBuffer(IBDesc);

                nvrhi::CommandListHandle GeomCmdList = NvrhiDevice->createCommandList();
                GeomCmdList->open();
                GeomCmdList->beginTrackingBufferState(DrawData.IndexBuffer, nvrhi::ResourceStates::CopyDest);
                GeomCmdList->writeBuffer(DrawData.IndexBuffer, Indices.data(), IBDesc.byteSize);
                GeomCmdList->setPermanentBufferState(DrawData.IndexBuffer, nvrhi::ResourceStates::IndexBuffer);
                GeomCmdList->close();
                NvrhiDevice->executeCommandList(GeomCmdList);
            }

            AllMeshDrawData.push_back(DrawData);
        }
        HLVM_LOG(LogTest, info, TXT("Created geometry buffers for {} meshes"), AllMeshDrawData.size());

        // =====================================================================
        // Create GBuffer input layout (uses offsetof)
        // =====================================================================
        {
            nvrhi::VertexAttributeDesc Attrs[4];
            Attrs[0].setName("POSITION").setFormat(nvrhi::Format::RGB32_FLOAT)
                .setOffset(offsetof(FVertex, Position)).setElementStride(sizeof(FVertex));
            Attrs[1].setName("NORMAL").setFormat(nvrhi::Format::RGB32_FLOAT)
                .setOffset(offsetof(FVertex, Normal)).setElementStride(sizeof(FVertex));
            Attrs[2].setName("TEXCOORD0").setFormat(nvrhi::Format::RG32_FLOAT)
                .setOffset(offsetof(FVertex, UV)).setElementStride(sizeof(FVertex));
            Attrs[3].setName("TANGENT").setFormat(nvrhi::Format::RGB32_FLOAT)
                .setOffset(offsetof(FVertex, Tangent)).setElementStride(sizeof(FVertex));

            GBufferInputLayout = NvrhiDevice->createInputLayout(Attrs, 4, GBufferVS);
        }

        // =====================================================================
        // Create GBuffer binding layout
        // =====================================================================
        {
            nvrhi::BindingLayoutDesc LayoutDesc;
            LayoutDesc.visibility = nvrhi::ShaderType::Vertex | nvrhi::ShaderType::Pixel;

            nvrhi::VulkanBindingOffsets offsets;
            offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
            LayoutDesc.setBindingOffsets(offsets);

            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::ConstantBuffer(256), // ViewConstants b0
                nvrhi::BindingLayoutItem::Texture_SRV(0),      // DiffuseTexture t0
                nvrhi::BindingLayoutItem::Sampler(128)         // LinearSampler s0
            };

            GBufferBindingLayout = NvrhiDevice->createBindingLayout(LayoutDesc);
        }

        // =====================================================================
        // Create GBuffer constant buffer
        // =====================================================================
        {
            nvrhi::BufferDesc CBDesc;
            CBDesc.byteSize = sizeof(float) * 16 * 4 + sizeof(float) * 4; // Model(4) + View(4) + Proj(4) + CameraPos(1)
            CBDesc.isConstantBuffer = true;
            CBDesc.isVolatile = false;
            CBDesc.keepInitialState = true;
            CBDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
            CBDesc.debugName = "ViewConstants";
            ViewConstantsBuffer = NvrhiDevice->createBuffer(CBDesc);
        }

        // =====================================================================
        // Create GBuffer sampler
        // =====================================================================
        {
            nvrhi::SamplerDesc SamplerDesc;
            SamplerDesc.setAddressU(nvrhi::SamplerAddressMode::Repeat)
                .setAddressV(nvrhi::SamplerAddressMode::Repeat)
                .setAddressW(nvrhi::SamplerAddressMode::Repeat)
                .setMinFilter(true)
                .setMagFilter(true)
                .setMipFilter(true);
            GBufferLinearSampler = NvrhiDevice->createSampler(SamplerDesc);
        }

        // =====================================================================
        // Create placeholder texture (1x1 white)
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
            PlaceholderTexture = NvrhiDevice->createTexture(Desc);

            nvrhi::CommandListHandle TexCmdList = NvrhiDevice->createCommandList();
            TexCmdList->open();
            uint32_t whitePixel = 0xFFFFFFFF;
            TexCmdList->writeTexture(PlaceholderTexture, 0, 0, &whitePixel, 4);
            TexCmdList->close();
            NvrhiDevice->executeCommandList(TexCmdList);
        }

        // =====================================================================
        // Create GBuffer pipeline
        // =====================================================================
        {
            nvrhi::GraphicsPipelineDesc PipelineDesc;
            PipelineDesc.setVertexShader(GBufferVS);
            PipelineDesc.setPixelShader(GBufferPS);
            PipelineDesc.setInputLayout(GBufferInputLayout);
            PipelineDesc.addBindingLayout(GBufferBindingLayout);
            PipelineDesc.setPrimType(nvrhi::PrimitiveType::TriangleList);
            PipelineDesc.renderState.setRasterState(nvrhi::RasterState().setCullNone());
            PipelineDesc.renderState.depthStencilState
                .setDepthTestEnable(true)
                .setDepthWriteEnable(true)
                .setDepthFunc(nvrhi::ComparisonFunc::Less);

            GBufferPipeline = NvrhiDevice->createGraphicsPipeline(PipelineDesc, GBufferFramebuffer->getFramebufferInfo());
            if (!GBufferPipeline)
            {
                HLVM_LOG(LogTest, err, TXT("Failed to create GBuffer pipeline"));
                return false;
            }
        }
        HLVM_LOG(LogTest, info, TXT("GBuffer pipeline created"));

        // =====================================================================
        // Create RT binding layout (reads GBuffer textures)
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Creating RT binding layout..."));
        {
            nvrhi::BindingLayoutDesc LayoutDesc;
            LayoutDesc.visibility = nvrhi::ShaderType::All;

            nvrhi::VulkanBindingOffsets offsets;
            offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
            LayoutDesc.setBindingOffsets(offsets);

            // b0: LightingConstants -> 256 (bRegShift=256)
            // t0: SceneBVH -> 0
            // t1: GBufferWorldPos -> 1
            // t2: GBufferNormals -> 2
            // t3: GBufferDiffuse -> 3
            // u0: Output -> 384 (uRegShift=384)
            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::ConstantBuffer(256),
                nvrhi::BindingLayoutItem::RayTracingAccelStruct(0),
                nvrhi::BindingLayoutItem::Texture_SRV(1),
                nvrhi::BindingLayoutItem::Texture_SRV(2),
                nvrhi::BindingLayoutItem::Texture_SRV(3),
                nvrhi::BindingLayoutItem::Texture_UAV(384)
            };

            RTBindingLayout = NvrhiDevice->createBindingLayout(LayoutDesc);
            if (!RTBindingLayout)
            {
                HLVM_LOG(LogTest, err, TXT("Failed to create RT binding layout"));
                return false;
            }
        }

        // =====================================================================
        // Create RT pipeline
        // =====================================================================
        {
            nvrhi::rt::PipelineDesc PipelineDesc;
            PipelineDesc.globalBindingLayouts = { RTBindingLayout };
            PipelineDesc.shaders = {
                { "", RayGenShader, nullptr },
                { "", MissShader, nullptr },
                { "", ShadowMissShader, nullptr }
            };
            PipelineDesc.hitGroups = { { "HitGroup",
                ClosestHitShader,
                nullptr,
                nullptr,
                nullptr,
                false } };
            PipelineDesc.maxPayloadSize = sizeof(float) * 4;

            RTPipeline = NvrhiDevice->createRayTracingPipeline(PipelineDesc);
            if (!RTPipeline)
            {
                HLVM_LOG(LogTest, err, TXT("Failed to create RT pipeline"));
                return false;
            }
            HLVM_LOG(LogTest, info, TXT("RT pipeline created"));
        }

        // =====================================================================
        // Create shader table
        // =====================================================================
        ShaderTable = RTPipeline->createShaderTable();
        ShaderTable->setRayGenerationShader("RayGen");
        ShaderTable->addHitGroup("HitGroup");
        ShaderTable->addMissShader("Miss");
        ShaderTable->addMissShader("ShadowMiss");

        // =====================================================================
        // Create HDR output texture
        // =====================================================================
        {
            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = GBufferWidth;
            Desc.height = GBufferHeight;
            Desc.format = nvrhi::Format::RGBA32_FLOAT;
            Desc.isRenderTarget = false;
            Desc.isUAV = true;
            Desc.isTypeless = false;
            Desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
            Desc.keepInitialState = true;
            Desc.debugName = "HDROutput";
            HDRTexture = NvrhiDevice->createTexture(Desc);
        }

        // =====================================================================
        // Create lighting constant buffer
        // =====================================================================
        {
            nvrhi::BufferDesc BufferDesc;
            BufferDesc.byteSize = sizeof(float) * 16; // 4 float4s
            BufferDesc.isConstantBuffer = true;
            BufferDesc.isVolatile = false;
            BufferDesc.keepInitialState = true;
            BufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
            BufferDesc.debugName = "LightingConstants";
            LightingConstantBuffer = NvrhiDevice->createBuffer(BufferDesc);
        }

        // =====================================================================
        // Create staging texture for frame dumps
        // =====================================================================
        {
            nvrhi::TextureDesc StagingDesc;
            StagingDesc.dimension = nvrhi::TextureDimension::Texture2D;
            StagingDesc.width = GBufferWidth;
            StagingDesc.height = GBufferHeight;
            StagingDesc.format = nvrhi::Format::RGBA32_FLOAT;
            StagingDesc.isRenderTarget = false;
            StagingDesc.isUAV = false;
            StagingDesc.isTypeless = false;
            StagingDesc.initialState = nvrhi::ResourceStates::CopyDest;
            StagingDesc.keepInitialState = false;
            StagingDesc.debugName = "StagingTexture";
            StagingTexture = NvrhiDevice->createStagingTexture(StagingDesc, nvrhi::CpuAccessMode::Read);
        }

        HLVM_LOG(LogTest, info, TXT("FRTShadowsGBufferPass initialized successfully"));
        return true;
    }

    void Shutdown()
    {
        HLVM_LOG(LogTest, info, TXT("FRTShadowsGBufferPass::Shutdown"));

        BindingCache.Clear();

        CommandList = nullptr;
        RTBindingLayout = nullptr;
        ShaderTable = nullptr;
        RTPipeline = nullptr;
        TopLevelAS = nullptr;
        ShaderLibrary = nullptr;
        RayGenShader = nullptr;
        ClosestHitShader = nullptr;
        MissShader = nullptr;
        ShadowMissShader = nullptr;
        HDRTexture = nullptr;
        RTBindingSet = nullptr;
        LightingConstantBuffer = nullptr;
        StagingTexture = nullptr;
        MeshBLASHandles.clear();
        TLASBuilder.Reset();

        GBufferVS = nullptr;
        GBufferPS = nullptr;
        GBufferDiffuseTexture = nullptr;
        GBufferSpecularTexture = nullptr;
        GBufferNormalsTexture = nullptr;
        GBufferEmissiveTexture = nullptr;
        GBufferWorldPosTexture = nullptr;
        GBufferDepthTexture = nullptr;
        GBufferFramebuffer = nullptr;
        PlaceholderTexture = nullptr;

        AllMeshDrawData.clear();

        GBufferInputLayout = nullptr;
        GBufferBindingLayout = nullptr;
        ViewConstantsBuffer = nullptr;
        GBufferPipeline = nullptr;
        GBufferLinearSampler = nullptr;
    }

    virtual void Animate(float fElapsedTimeSeconds) override
    {
        FrameCount++;
        FPSUpdateTimer += fElapsedTimeSeconds;
        float FPS = float(FrameCount) / FPSUpdateTimer;
        if (FPSUpdateTimer >= 1.0f)
        {
            WindowTitle = FString::Format(TXT("RT Shadows GBuffer - FPS: {:.1f}"), FPS);

            if (auto* DM = GetDeviceManager())
            {
                DM->SetWindowTitle(WindowTitle);
            }
            FPSUpdateTimer = 0.0f;
            FrameCount = 0;
        }
    }

    virtual void Render(nvrhi::IFramebuffer* Framebuffer) override
    {
        if (!NvrhiDevice || !Framebuffer)
            return;

        const auto& CurrentFBInfo = Framebuffer->getFramebufferInfo();

        // =====================================================================
        // Resize handling for GBuffer textures
        // =====================================================================
        if (!GBufferNormalsTexture || CurrentFBInfo.width != LastWidth || CurrentFBInfo.height != LastHeight)
        {
            HLVM_LOG(LogTest, info, TXT("Resizing GBuffer - width: {}, height: {}"), CurrentFBInfo.width, CurrentFBInfo.height);
            LastWidth = CurrentFBInfo.width;
            LastHeight = CurrentFBInfo.height;

            uint32_t GBufferWidth = CurrentFBInfo.width;
            uint32_t GBufferHeight = CurrentFBInfo.height;

            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = GBufferWidth;
            Desc.height = GBufferHeight;
            Desc.isRenderTarget = true;
            Desc.isUAV = false;
            Desc.initialState = nvrhi::ResourceStates::RenderTarget;
            Desc.keepInitialState = true;

            Desc.format = nvrhi::Format::RGBA8_UNORM;
            Desc.debugName = "GBufferDiffuse";
            GBufferDiffuseTexture = NvrhiDevice->createTexture(Desc);

            Desc.format = nvrhi::Format::RGBA16_FLOAT;
            Desc.debugName = "GBufferSpecular";
            GBufferSpecularTexture = NvrhiDevice->createTexture(Desc);

            Desc.debugName = "GBufferNormals";
            GBufferNormalsTexture = NvrhiDevice->createTexture(Desc);

            Desc.debugName = "GBufferEmissive";
            GBufferEmissiveTexture = NvrhiDevice->createTexture(Desc);

            Desc.debugName = "GBufferWorldPos";
            GBufferWorldPosTexture = NvrhiDevice->createTexture(Desc);

            Desc.format = nvrhi::Format::D32;
            Desc.isTypeless = true;
            Desc.initialState = nvrhi::ResourceStates::DepthWrite;
            Desc.debugName = "GBufferDepth";
            GBufferDepthTexture = NvrhiDevice->createTexture(Desc);

            nvrhi::FramebufferDesc FBDesc;
            FBDesc.addColorAttachment(GBufferDiffuseTexture);
            FBDesc.addColorAttachment(GBufferSpecularTexture);
            FBDesc.addColorAttachment(GBufferNormalsTexture);
            FBDesc.addColorAttachment(GBufferEmissiveTexture);
            FBDesc.addColorAttachment(GBufferWorldPosTexture);
            FBDesc.setDepthAttachment(GBufferDepthTexture);
            GBufferFramebuffer = NvrhiDevice->createFramebuffer(FBDesc);

            // Recreate GBuffer pipeline with new framebuffer info
            {
                nvrhi::GraphicsPipelineDesc PipelineDesc;
                PipelineDesc.setVertexShader(GBufferVS);
                PipelineDesc.setPixelShader(GBufferPS);
                PipelineDesc.setInputLayout(GBufferInputLayout);
                PipelineDesc.addBindingLayout(GBufferBindingLayout);
                PipelineDesc.setPrimType(nvrhi::PrimitiveType::TriangleList);
                PipelineDesc.renderState.setRasterState(nvrhi::RasterState().setCullNone());
                PipelineDesc.renderState.depthStencilState
                    .setDepthTestEnable(true)
                    .setDepthWriteEnable(true)
                    .setDepthFunc(nvrhi::ComparisonFunc::Less);
                GBufferPipeline = NvrhiDevice->createGraphicsPipeline(PipelineDesc, GBufferFramebuffer->getFramebufferInfo());
            }

            // Recreate HDR texture
            Desc.format = nvrhi::Format::RGBA32_FLOAT;
            Desc.isRenderTarget = false;
            Desc.isUAV = true;
            Desc.isTypeless = false;
            Desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
            Desc.debugName = "HDROutput";
            HDRTexture = NvrhiDevice->createTexture(Desc);

            // Recreate staging texture
            {
                nvrhi::TextureDesc StagingDesc;
                StagingDesc.dimension = nvrhi::TextureDimension::Texture2D;
                StagingDesc.width = GBufferWidth;
                StagingDesc.height = GBufferHeight;
                StagingDesc.format = nvrhi::Format::RGBA32_FLOAT;
                StagingDesc.isRenderTarget = false;
                StagingDesc.isUAV = false;
                StagingDesc.isTypeless = false;
                StagingDesc.initialState = nvrhi::ResourceStates::CopyDest;
                StagingDesc.keepInitialState = false;
                StagingDesc.debugName = "StagingTexture";
                StagingTexture = NvrhiDevice->createStagingTexture(StagingDesc, nvrhi::CpuAccessMode::Read);
            }

            BindingCache.Clear();
        }

        // =====================================================================
        // Single command list: GBuffer -> RT -> Blit
        // =====================================================================
        nvrhi::CommandListParameters CmdListParams;
        CmdListParams.enableImmediateExecution = false;
        nvrhi::CommandListHandle CmdList = NvrhiDevice->createCommandList(CmdListParams);
        CmdList->open();

        // =====================================================================
        // Static camera setup
        // =====================================================================
        glm::vec3 CameraPos = SceneCenter + glm::vec3(0.0f, 8.0f, 20.0f);
        glm::mat4 view = glm::lookAtLH(CameraPos, SceneCenter, glm::vec3(0.0f, 1.0f, 0.0f));
        float aspectRatio = float(CurrentFBInfo.width) / float(CurrentFBInfo.height);
        glm::mat4 proj = glm::perspectiveLH_ZO(glm::radians(90.0f), aspectRatio, 0.1f, 1000.0f);

        // Write ViewConstants (column-major via glm::value_ptr)
        float ViewConstantsData[16 * 4];
        memcpy(&ViewConstantsData[0], glm::value_ptr(glm::scale(glm::mat4(1.0f), glm::vec3(2.0f))), 64);
        memcpy(&ViewConstantsData[16], glm::value_ptr(view), 64);
        memcpy(&ViewConstantsData[32], glm::value_ptr(proj), 64);
        ViewConstantsData[48] = CameraPos.x;
        ViewConstantsData[49] = CameraPos.y;
        ViewConstantsData[50] = CameraPos.z;
        ViewConstantsData[51] = 1.0f;
        CmdList->writeBuffer(ViewConstantsBuffer, ViewConstantsData, sizeof(ViewConstantsData));

        // =====================================================================
        // GBuffer Pass: Clear and draw all meshes
        // =====================================================================
        nvrhi::Color clearBlack(0.f, 0.f, 0.f, 0.f);
        nvrhi::Color clearBlue(0.f, 0.f, 1.f, 1.f);
        nvrhi::Color clearNormalUp(0.5f, 1.0f, 0.5f, 1.f);
        nvrhi::utils::ClearColorAttachment(CmdList, GBufferFramebuffer, 0, clearBlue);
        nvrhi::utils::ClearColorAttachment(CmdList, GBufferFramebuffer, 1, clearBlack);
        nvrhi::utils::ClearColorAttachment(CmdList, GBufferFramebuffer, 2, clearNormalUp);
        nvrhi::utils::ClearColorAttachment(CmdList, GBufferFramebuffer, 3, clearBlack);
        nvrhi::utils::ClearColorAttachment(CmdList, GBufferFramebuffer, 4, clearBlack);
        nvrhi::utils::ClearDepthStencilAttachment(CmdList, GBufferFramebuffer, 1.0f, 0u);

        for (size_t MeshIdx = 0; MeshIdx < AllMeshDrawData.size(); ++MeshIdx)
        {
            const auto& DrawData = AllMeshDrawData[MeshIdx];

            // Look up material texture for this mesh
            nvrhi::TextureHandle DiffuseTex = PlaceholderTexture;
            if (DrawData.Mesh && Scene)
            {
                auto it = Scene->MeshMultiMaterialMap.find(DrawData.Mesh);
                if (it != Scene->MeshMultiMaterialMap.end())
                {
                    auto& materials = it->second;
                    if (!materials.empty())
                    {
                        if (auto PBRMat = std::dynamic_pointer_cast<FPBRMaterial>(materials[0]))
                        {
                            if (PBRMat->HasGPUTexture(IMaterial::ETextureType::Albedo))
                            {
                                DiffuseTex = PBRMat->GetGPUTexture(IMaterial::ETextureType::Albedo).GetTextureSRV();
                                if (!DiffuseTex)
                                {
                                    DiffuseTex = PlaceholderTexture;
                                }
                            }
                        }
                    }
                }
            }

            nvrhi::BindingSetDesc GBufferBindingSetDesc;
            GBufferBindingSetDesc.bindings = {
                nvrhi::BindingSetItem::ConstantBuffer(256, ViewConstantsBuffer),
                nvrhi::BindingSetItem::Texture_SRV(0, DiffuseTex),
                nvrhi::BindingSetItem::Sampler(128, GBufferLinearSampler)
            };
            nvrhi::BindingSetHandle GBufferBindingSet = BindingCache.GetOrCreateBindingSet(GBufferBindingSetDesc, GBufferBindingLayout);

            nvrhi::GraphicsState State;
            State.setPipeline(GBufferPipeline);
            State.setFramebuffer(GBufferFramebuffer);
            State.addBindingSet(GBufferBindingSet);

            nvrhi::VertexBufferBinding VBBinding;
            VBBinding.setBuffer(DrawData.VertexBuffer);
            VBBinding.setSlot(0);
            VBBinding.setOffset(0);
            State.addVertexBuffer(VBBinding);

            nvrhi::IndexBufferBinding IBBinding;
            IBBinding.setBuffer(DrawData.IndexBuffer);
            IBBinding.setOffset(0);
            IBBinding.setFormat(nvrhi::Format::R32_UINT);
            State.setIndexBuffer(IBBinding);

            nvrhi::Viewport viewport(0.f, float(CurrentFBInfo.width), 0.f, float(CurrentFBInfo.height), 0.0f, 1.0f);
            State.viewport.addViewportAndScissorRect(viewport);

            CmdList->setGraphicsState(State);

            nvrhi::DrawArguments DrawArgs;
            DrawArgs.vertexCount = DrawData.IndexCount;
            CmdList->drawIndexed(DrawArgs);
        }

        if (FrameCount < 2)
        {
            HLVM_LOG(LogTest, info, TXT("GBuffer Pass: drew {} meshes"), AllMeshDrawData.size());
        }

        // =====================================================================
        // Transition GBuffer textures to ShaderResource for RT pass
        // =====================================================================
        CmdList->setTextureState(GBufferDiffuseTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(GBufferSpecularTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(GBufferNormalsTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(GBufferEmissiveTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(GBufferWorldPosTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(GBufferDepthTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

        // =====================================================================
        // RT Shadow Pass
        // =====================================================================

        // Rotating light direction (use separate counter to avoid FPS reset)
        static uint32_t LightFrameCount = 0;
        LightFrameCount++;
        float LightAngle = static_cast<float>(LightFrameCount) * 0.05f;
        glm::vec3 LightDir = glm::normalize(glm::vec3(sinf(LightAngle), 0.6f, cosf(LightAngle)));
        float LightingData[16] = {
            LightDir.x, LightDir.y, LightDir.z, 1.0f,      // LightDir + intensity
            0.3f, 0.3f, 0.35f, 0.0f,                       // Ambient color (slightly blue)
            0.0f, 0.0f, 0.0f, 0.0f,                        // padding
            0.0f, 0.0f, 0.0f, 0.0f                         // padding
        };
        CmdList->writeBuffer(LightingConstantBuffer, LightingData, sizeof(LightingData));

        // Transition HDR to UAV for RT write
        CmdList->setTextureState(HDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

        // Create RT binding set
        nvrhi::BindingSetDesc RTBindingSetDesc;
        RTBindingSetDesc.bindings = {
            nvrhi::BindingSetItem::ConstantBuffer(256, LightingConstantBuffer),
            nvrhi::BindingSetItem::RayTracingAccelStruct(0, TopLevelAS),
            nvrhi::BindingSetItem::Texture_SRV(1, GBufferWorldPosTexture),
            nvrhi::BindingSetItem::Texture_SRV(2, GBufferNormalsTexture),
            nvrhi::BindingSetItem::Texture_SRV(3, GBufferDiffuseTexture),
            nvrhi::BindingSetItem::Texture_UAV(384, HDRTexture)
        };
        RTBindingSet = BindingCache.GetOrCreateBindingSet(RTBindingSetDesc, RTBindingLayout);

        nvrhi::rt::State RTState;
        RTState.shaderTable = ShaderTable;
        RTState.bindings = { RTBindingSet };
        CmdList->setRayTracingState(RTState);

        nvrhi::rt::DispatchRaysArguments args;
        args.width = CurrentFBInfo.width;
        args.height = CurrentFBInfo.height;
        CmdList->dispatchRays(args);

        // =====================================================================
        // Frame dump: copy HDR to staging (before blit, on same command list)
        // =====================================================================
        static bool dumpRequested = !!getenv("HLVM_DUMP_RT");
        static int framesToDump = 0;
        bool doDumpThisFrame = dumpRequested && framesToDump < 4;
        if (doDumpThisFrame) {
            framesToDump++;
            CmdList->setTextureState(HDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopySource);
            nvrhi::TextureSlice slice = {};
            slice.width = CurrentFBInfo.width;
            slice.height = CurrentFBInfo.height;
            slice.depth = 1;
            CmdList->copyTexture(StagingTexture.Get(), slice, HDRTexture.Get(), slice);
        }

        // =====================================================================
        // Always blit HDR texture to screen
        // =====================================================================
        CmdList->setTextureState(HDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        FCommonRenderPasses::BlitParameters BlitParams;
        FCommonRenderPasses::BlitTexture(
            CmdList,
            Framebuffer,
            HDRTexture,
            &BindingCache,
            CurrentFBInfo.width,
            CurrentFBInfo.height,
            BlitParams);

        CmdList->close();
        NvrhiDevice->executeCommandList(CmdList);
        NvrhiDevice->waitForIdle();

        // =====================================================================
        // Readback and save frame dump (after GPU is idle)
        // =====================================================================
        if (doDumpThisFrame) {
            nvrhi::TextureSlice slice = {};
            slice.width = CurrentFBInfo.width;
            slice.height = CurrentFBInfo.height;
            slice.depth = 1;

            size_t rowPitch = 0;
            void* mappedData = NvrhiDevice->mapStagingTexture(
                StagingTexture.Get(), slice, nvrhi::CpuAccessMode::Read, &rowPitch);

            if (mappedData) {
                uint32_t imgWidth = CurrentFBInfo.width;
                uint32_t imgHeight = CurrentFBInfo.height;

                std::vector<float> pixels(static_cast<size_t>(imgWidth) * imgHeight * 4);
                uint8_t* srcRow = reinterpret_cast<uint8_t*>(mappedData);
                float* dst = pixels.data();

                for (uint32_t y = 0; y < imgHeight; y++) {
                    float* src = reinterpret_cast<float*>(srcRow + static_cast<size_t>(imgHeight - 1 - y) * rowPitch);
                    for (uint32_t x = 0; x < imgWidth; x++) {
                        size_t dstIdx = (static_cast<size_t>(y) * imgWidth + x) * 4;
                        size_t srcIdx = x * 4;
                        dst[dstIdx + 0] = src[srcIdx + 0];
                        dst[dstIdx + 1] = src[srcIdx + 1];
                        dst[dstIdx + 2] = src[srcIdx + 2];
                        dst[dstIdx + 3] = src[srcIdx + 3];
                    }
                }

                NvrhiDevice->unmapStagingTexture(StagingTexture.Get());

                // Use GProjectRoot for correct path, create dir if needed
                FString DumpDir = FString::Format(TXT("{}/Engine/Source/Runtime/Test/TestRTShadowsGBuffer_Data/dumps"), *GProjectRoot);
                std::filesystem::create_directories(FPath(DumpDir).string());
                FString baseFilename = FImageDump::GenerateTimestampedFilename(DumpDir);
                FString filename = FString::Format(TXT("{}_frame{:04d}.png"), baseFilename.substr(0, baseFilename.length() - 4).c_str(), framesToDump);
                if (FImageDump::DumpToPNG(filename, static_cast<int>(imgWidth), static_cast<int>(imgHeight), pixels.data())) {
                    HLVM_LOG(LogTest, info, TXT("Dumped frame {} to {}"), framesToDump, *filename);
                } else {
                    HLVM_LOG(LogTest, critical, TXT("Failed to dump frame to {}"), *filename);
                }
            }

            if (framesToDump == 4) {
                return;
            }
        }
    }

    virtual void BackBufferResizing() override
    {
        HDRTexture = nullptr;
        StagingTexture = nullptr;
        RTBindingSet = nullptr;
        GBufferFramebuffer = nullptr;
        GBufferDiffuseTexture = nullptr;
        GBufferSpecularTexture = nullptr;
        GBufferNormalsTexture = nullptr;
        GBufferEmissiveTexture = nullptr;
        GBufferWorldPosTexture = nullptr;
        GBufferDepthTexture = nullptr;
        BindingCache.Clear();
    }

private:
    nvrhi::IDevice*        NvrhiDevice = nullptr;
    nvrhi::FramebufferInfo FBInfo;
    FString                WindowTitle;

    // Scene
    std::shared_ptr<FScene3DNode> Scene;
    glm::vec3 SceneCenter = glm::vec3(0.f);

    // RT shaders
    nvrhi::ShaderLibraryHandle ShaderLibrary;
    nvrhi::ShaderHandle        RayGenShader;
    nvrhi::ShaderHandle        ClosestHitShader;
    nvrhi::ShaderHandle        MissShader;
    nvrhi::ShaderHandle        ShadowMissShader;

    // Acceleration structures
    nvrhi::rt::AccelStructHandle          TopLevelAS;
    TVector<nvrhi::rt::AccelStructHandle> MeshBLASHandles;
    FTLASBuilder                          TLASBuilder;

    // RT pipeline
    nvrhi::rt::PipelineHandle    RTPipeline;
    nvrhi::rt::ShaderTableHandle ShaderTable;
    nvrhi::BindingLayoutHandle   RTBindingLayout;
    nvrhi::BindingSetHandle      RTBindingSet;
    nvrhi::TextureHandle         HDRTexture;

    // Constant buffer for lighting
    nvrhi::BufferHandle LightingConstantBuffer;

    // GBuffer shaders
    nvrhi::ShaderHandle GBufferVS;
    nvrhi::ShaderHandle GBufferPS;

    // GBuffer textures
    nvrhi::TextureHandle GBufferDiffuseTexture;
    nvrhi::TextureHandle GBufferSpecularTexture;
    nvrhi::TextureHandle GBufferNormalsTexture;
    nvrhi::TextureHandle GBufferEmissiveTexture;
    nvrhi::TextureHandle GBufferWorldPosTexture;
    nvrhi::TextureHandle GBufferDepthTexture;
    nvrhi::SamplerHandle GBufferLinearSampler;
    nvrhi::TextureHandle PlaceholderTexture;

    // GBuffer framebuffer
    nvrhi::FramebufferHandle GBufferFramebuffer;

    // Per-mesh data for multi-mesh rendering
    struct FMeshDrawData {
        nvrhi::BufferHandle VertexBuffer;
        nvrhi::BufferHandle IndexBuffer;
        std::shared_ptr<FStaticMesh> Mesh;
        uint32_t IndexCount;
    };
    TVector<FMeshDrawData> AllMeshDrawData;

    // GBuffer pipeline
    nvrhi::InputLayoutHandle      GBufferInputLayout;
    nvrhi::BindingLayoutHandle    GBufferBindingLayout;
    nvrhi::BufferHandle           ViewConstantsBuffer;
    nvrhi::GraphicsPipelineHandle GBufferPipeline;

    FBindingCache            BindingCache;
    nvrhi::CommandListHandle CommandList;

    // Staging texture for frame dumps
    nvrhi::StagingTextureHandle StagingTexture;

    uint32_t LastWidth = 0;
    uint32_t LastHeight = 0;
    uint32_t FrameCount = 0;
    float    FPSUpdateTimer = 0.0f;
};

// =============================================================================
// TEST IMPLEMENTATION
// =============================================================================

RECORD_BOOL(test_RTShadowsGBuffer)
{
    HLVM_LOG(LogTest, info, TXT("=== Starting RT Shadows GBuffer Test ==="));

    try
    {
        HLVM_LOG(LogTest, info, TXT("Creating window..."));
        IWindow::Properties WindowProps;
        WindowProps.Title = WINDOW_TITLE;
        WindowProps.Extent = { WINDOW_WIDTH, WINDOW_HEIGHT };
        WindowProps.Resizable = true;
        WindowProps.VSync = IWindow::EVsync::Off;

        HLVM_LOG(LogTest, info, TXT("Creating DeviceManager..."));
        TUniquePtr<FDeviceManager> DeviceManager = FDeviceManager::Create(nvrhi::GraphicsAPI::VULKAN);
        if (!DeviceManager)
        {
            throw std::runtime_error("Failed to create DeviceManager");
        }

        FDeviceCreationParameters& DeviceParams = const_cast<FDeviceCreationParameters&>(
            DeviceManager->GetDeviceParams());
        DeviceParams.BackBufferWidth = WINDOW_WIDTH;
        DeviceParams.BackBufferHeight = WINDOW_HEIGHT;
        DeviceParams.SwapChainBufferCount = 2;
        DeviceParams.VSyncMode = 0;
        DeviceParams.bEnableDebugRuntime = false;
        DeviceParams.bEnableNVRHIValidationLayer = true;
        DeviceParams.bEnableRayTracingExtensions = true;
        // Suppress known validation layer false positive: dynamic viewport/scissor state from prior
        // graphics pipeline is incorrectly attributed to ray-tracing pipeline dispatch.
        // See: https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/8038
        DeviceParams.IgnoredVulkanValidationMessageLocations.push_back(0x29056f6a);


        if (!DeviceManager->CreateWindowDeviceAndSwapChain(WindowProps))
        {
            throw std::runtime_error("Failed to create window, device and swap chain");
        }

        HLVM_LOG(LogTest, info, TXT("Device created with ray tracing enabled"));

        nvrhi::IDevice* NvrhiDevice = DeviceManager->GetDevice();
        if (!NvrhiDevice->queryFeatureSupport(nvrhi::Feature::RayTracingPipeline))
        {
            throw std::runtime_error("Ray tracing not supported on this device");
        }

        nvrhi::IFramebuffer* FirstFB = DeviceManager->GetFramebuffer(0);

        HLVM_LOG(LogTest, info, TXT("Creating render pass..."));
        TSharedPtr<FRTShadowsGBufferPass> RTPass =
            std::make_shared<FRTShadowsGBufferPass>(DeviceManager.get());
        if (!RTPass->Initialize(NvrhiDevice, FirstFB, FString(TXT("RT Shadows GBuffer Test"))))
        {
            throw std::runtime_error("Failed to initialize FRTShadowsGBufferPass");
        }

        DeviceManager->AddRenderPassToBack(RTPass);

        HLVM_LOG(LogTest, info, TXT("Starting render loop..."));

        std::thread([&]() {
            FTimer Timer;
            while (Timer.MarkSec() < 3.0)
            {
            }
            DeviceManager->StopMessageLoop();
        }).detach();

        DeviceManager->RunMessageLoop();

        RTPass->Shutdown();

        HLVM_LOG(LogTest, info, TXT("Test completed successfully!"));
        return true;
    }
    catch (const std::exception& e)
    {
        HLVM_LOG(LogTest, critical, TXT("Test failed: {}"), TO_TCHAR_CSTR(e.what()));
        return false;
    }
    catch (...)
    {
        HLVM_LOG(LogTest, critical, TXT("Unknown fatal error occurred"));
        return false;
    }
}

#else // HLVM_VULKAN_RENDERER

RECORD_BOOL(test_RTShadowsGBuffer)
{
    HLVM_LOG(LogTest, warning, TXT("Vulkan renderer not enabled - skipping test"));
    return true;
}

#endif // HLVM_VULKAN_RENDERER
