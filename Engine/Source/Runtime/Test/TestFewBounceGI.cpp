/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * Few-bounce GI Test - Sequential Bounce Tracing for RESTIR GI
 *
 * Pipeline:
 * 1. Load Sponza scene (27 meshes) with glTF textures
 * 2. GBuffer Pass: Render all meshes to 5 MRTs (Diffuse, Specular, Normal, Emissive, WorldPos)
 * 3. GI Pass: Sequential bounce tracing (2-4 bounces), cosine-weighted sampling, miss=black
 * 4. Blit Pass: Copy GI output to swapchain
 *
 * Purpose: Provide few-bounce GI infrastructure for RESTIR.
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
#include "Renderer/Texture/TextureCache.h"
#include "Renderer/RayTracing/BLASBuilder.h"
#include "Renderer/RayTracing/TLASBuilder.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
#include "Renderer/PostProcess/FBilateralDenoisePass.h"
#include "Renderer/PostProcess/FReBLURPass.h"
#include "Renderer/PostProcess/FReSTIRPass.h"
#include "Image/FImageDump.h"
#include "Core/Parallel/Async/WorkStealThreadPool.h"
#include <nvrhi/utils.h>
#include <Utility/Timer.h>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

DECLARE_LOG_CATEGORY(LogTest)

static bool g_ReSTIRDebugVis = false;

#if HLVM_VULKAN_RENDERER

// =============================================================================
// CONFIGURATION
// =============================================================================

static const char*    WINDOW_TITLE = "Few-bounce GI Test";
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
// FFewBounceGIPass
// =============================================================================

class FFewBounceGIPass : public IRenderPass
{
public:
    using IRenderPass::IRenderPass;

    bool Initialize(nvrhi::IDevice* Device, nvrhi::IFramebuffer* Framebuffer, const FString& InWindowTitle)
    {
        HLVM_LOG(LogTest, info, TXT("=== FFewBounceGIPass::Initialize ==="));

        NvrhiDevice = Device;
        FBInfo = Framebuffer->getFramebufferInfo();
        WindowTitle = InWindowTitle;

        BindingCache.SetDevice(NvrhiDevice);

        const auto DataDir = FString::Format(
            TXT("{}/Engine/Source/Runtime/Test/TestFewBounceGI_Data"),
            *GProjectRoot);

        // =====================================================================
        // Load GI shader from ShaderMake blob
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Loading GI shader..."));
        auto GIShaderBlob = ReadBinaryFile(
            FPath::Combine(DataDir, TXT("FewBounceGI.sblob")).string());

        const void* GIShaderBinary = nullptr;
        size_t      GIShaderBinarySize = 0;
        if (!ShaderMake::FindPermutationInBlob(GIShaderBlob.data(), GIShaderBlob.size(), nullptr, 0, &GIShaderBinary, &GIShaderBinarySize))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to extract GI shader from blob"));
            return false;
        }

        ShaderLibrary = NvrhiDevice->createShaderLibrary(
            GIShaderBinary, GIShaderBinarySize);
        if (!ShaderLibrary)
        {
            HLVM_LOG(LogTest, err, TXT("Failed to load GI shader library"));
            return false;
        }

        RayGenShader = ShaderLibrary->getShader("RayGen", nvrhi::ShaderType::RayGeneration);
        ClosestHitShader = ShaderLibrary->getShader("ClosestHit", nvrhi::ShaderType::ClosestHit);
        MissShader = ShaderLibrary->getShader("Miss", nvrhi::ShaderType::Miss);

        if (!RayGenShader || !ClosestHitShader || !MissShader)
        {
            HLVM_LOG(LogTest, err, TXT("Failed to get GI shaders"));
            return false;
        }
        HLVM_LOG(LogTest, info, TXT("GI shaders loaded successfully"));

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

        // Add Sponza meshes as instances
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

        FAsyncTextureLoader::LoadMaterialTexturesAsync(
            NvrhiDevice, Materials,
            {IMaterial::ETextureType::Albedo});

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

        // MRT4: WorldPos (RGBA16F)
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
            CBDesc.byteSize = sizeof(float) * 16 * 3 + sizeof(float) * 4; // Model(16) + View(16) + Proj(16) + RenderTargetSize(2) + Padding(2)
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
        HLVM_LOG(LogTest, info, TXT("Creating GI binding layout..."));

        {
            nvrhi::BindingLayoutDesc LayoutDesc;
            LayoutDesc.visibility = nvrhi::ShaderType::All;

            nvrhi::VulkanBindingOffsets offsets;
            offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
            LayoutDesc.setBindingOffsets(offsets);

            // b0: GIConstants -> 256 (bRegShift=256)
            // b1: ViewConstants -> 512 (bRegShift=256, second buffer)
            // t0: SceneBVH -> 0
            // t1: GBufferWorldPos -> 1
            // t2: GBufferNormals -> 2
            // t3: GBufferDiffuse -> 3
            // u0: Output -> 384 (uRegShift=384)
            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::ConstantBuffer(256),
                nvrhi::BindingLayoutItem::ConstantBuffer(512),
                nvrhi::BindingLayoutItem::RayTracingAccelStruct(0),
                nvrhi::BindingLayoutItem::Texture_SRV(1),
                nvrhi::BindingLayoutItem::Texture_SRV(2),
                nvrhi::BindingLayoutItem::Texture_SRV(3),
                nvrhi::BindingLayoutItem::Texture_UAV(384)
            };

            RTBindingLayout = NvrhiDevice->createBindingLayout(LayoutDesc);
            if (!RTBindingLayout)
            {
                HLVM_LOG(LogTest, err, TXT("Failed to create GI binding layout"));
                return false;
            }
        }

        // =====================================================================
        // Create GI pipeline
        // =====================================================================
        {
            nvrhi::rt::PipelineDesc PipelineDesc;
            PipelineDesc.globalBindingLayouts = { RTBindingLayout };
            PipelineDesc.shaders = {
                { "", RayGenShader, nullptr },
                { "", MissShader, nullptr }
            };
            PipelineDesc.hitGroups = { { "HitGroup",
                ClosestHitShader,
                nullptr,
                nullptr,
                nullptr,
                false } };
            PipelineDesc.maxPayloadSize = sizeof(float) * 12; // throughput(3) + radiance(3) + origin(3) + direction(3) + bounceCount(1) + flags(1) ~= 14 floats

            RTPipeline = NvrhiDevice->createRayTracingPipeline(PipelineDesc);
            if (!RTPipeline)
            {
                HLVM_LOG(LogTest, err, TXT("Failed to create GI pipeline"));
                return false;
            }
            HLVM_LOG(LogTest, info, TXT("GI pipeline created"));
        }

        // =====================================================================
        // Create shader table
        // =====================================================================
        ShaderTable = RTPipeline->createShaderTable();
        ShaderTable->setRayGenerationShader("RayGen");
        ShaderTable->addHitGroup("HitGroup");
        ShaderTable->addMissShader("Miss");

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
        // Create denoised HDR texture (output of bilateral denoise)
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
            Desc.debugName = "DenoisedHDROutput";
            DenoisedHDRTexture = NvrhiDevice->createTexture(Desc);
        }

        // =====================================================================
        // Initialize denoise pass
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Initializing bilateral denoise pass..."));
        if (!DenoisePass.Initialize(NvrhiDevice, DataDir))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to initialize DenoisePass"));
            return false;
        }

        // =====================================================================
        // Create ReBLUR textures and initialize ReBLUR pass
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Creating ReBLUR textures..."));
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
            Desc.debugName = "ReBLURHistoryTexture";
            ReBLURHistoryTexture = NvrhiDevice->createTexture(Desc);

            Desc.debugName = "ReBLUROutputTexture";
            ReBLUROutputTexture = NvrhiDevice->createTexture(Desc);
        }

        // Clear ReBLUR history texture to zero (uninitialized memory may contain NaN,
        // which would propagate through SpatialBlur and produce black output)
        {
            nvrhi::CommandListHandle ClearCmd = NvrhiDevice->createCommandList();
            ClearCmd->open();
            nvrhi::Color ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            ClearCmd->clearTextureFloat(ReBLURHistoryTexture, nvrhi::AllSubresources, ClearColor);
            ClearCmd->close();
            NvrhiDevice->executeCommandList(ClearCmd);
        }

        // Initialize ReBLUR pass
        if (!ReBLURPass.Initialize(NvrhiDevice, DataDir))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to initialize ReBLURPass"));
            return false;
        }
        bReBLURInitialized = true;
        HLVM_LOG(LogTest, info, TXT("ReBLUR pass initialized successfully"));

        // =====================================================================
        // Create ReSTIR reservoir textures and initialize ReSTIR pass
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Creating ReSTIR textures..."));
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
            Desc.debugName = "Reservoir0";
            Reservoir0Texture = NvrhiDevice->createTexture(Desc);

            Desc.debugName = "Reservoir1";
            Reservoir1Texture = NvrhiDevice->createTexture(Desc);

            Desc.debugName = "Reservoir0History";
            Reservoir0HistoryTexture = NvrhiDevice->createTexture(Desc);

            Desc.debugName = "Reservoir1History";
            Reservoir1HistoryTexture = NvrhiDevice->createTexture(Desc);

            Desc.debugName = "Reservoir0Merged";
            Reservoir0MergedTexture = NvrhiDevice->createTexture(Desc);

            Desc.debugName = "Reservoir1Merged";
            Reservoir1MergedTexture = NvrhiDevice->createTexture(Desc);

            Desc.debugName = "SpatialRadiance";
            SpatialRadianceTexture = NvrhiDevice->createTexture(Desc);

            Desc.debugName = "ReSTIRDebug";
            ReSTIRDebugTexture = NvrhiDevice->createTexture(Desc);
        }

        // Clear ReSTIR history textures to zero (avoid garbage M values on first frame)
        {
            nvrhi::CommandListHandle ClearCmd = NvrhiDevice->createCommandList();
            ClearCmd->open();
            nvrhi::Color ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            ClearCmd->clearTextureFloat(Reservoir0HistoryTexture, nvrhi::AllSubresources, ClearColor);
            ClearCmd->clearTextureFloat(Reservoir1HistoryTexture, nvrhi::AllSubresources, ClearColor);
            ClearCmd->close();
            NvrhiDevice->executeCommandList(ClearCmd);
        }

        // Initialize ReSTIR pass
        if (!ReSTIRPass.Initialize(NvrhiDevice, DataDir))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to initialize ReSTIRPass"));
            return false;
        }
        bReSTIRInitialized = true;
        HLVM_LOG(LogTest, info, TXT("ReSTIR pass initialized successfully"));

        // =====================================================================
        // Create GI constant buffer
        // =====================================================================
        {
            nvrhi::BufferDesc BufferDesc;
            BufferDesc.byteSize = sizeof(float) * 16; // 4 float4s
            BufferDesc.isConstantBuffer = true;
            BufferDesc.isVolatile = false;
            BufferDesc.keepInitialState = true;
            BufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
            BufferDesc.debugName = "GIConstants";
            GIConstantBuffer = NvrhiDevice->createBuffer(BufferDesc);
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

        HLVM_LOG(LogTest, info, TXT("FFewBounceGIPass initialized successfully"));
        return true;
    }

    void Shutdown()
    {
        HLVM_LOG(LogTest, info, TXT("FFewBounceGIPass::Shutdown"));

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
        HDRTexture = nullptr;
        DenoisedHDRTexture = nullptr;
        DenoisePass.Shutdown();
        if (bReBLURInitialized)
        {
            ReBLURPass.Shutdown();
            bReBLURInitialized = false;
        }
        ReBLURHistoryTexture = nullptr;
        ReBLUROutputTexture = nullptr;

        if (bReSTIRInitialized)
        {
            ReSTIRPass.Shutdown();
            bReSTIRInitialized = false;
        }
        Reservoir0Texture = nullptr;
        Reservoir1Texture = nullptr;
        Reservoir0HistoryTexture = nullptr;
        Reservoir1HistoryTexture = nullptr;
        Reservoir0MergedTexture = nullptr;
        Reservoir1MergedTexture = nullptr;
        SpatialRadianceTexture = nullptr;
        ReSTIRDebugTexture = nullptr;
        RTBindingSet = nullptr;
        GIConstantBuffer = nullptr;
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
        TemporalFrameCount++;
        FPSUpdateTimer += fElapsedTimeSeconds;
        float FPS = float(FrameCount) / FPSUpdateTimer;
        if (FPSUpdateTimer >= 1.0f)
        {
            WindowTitle = FString::Format(TXT("Few-bounce GI - FPS: {:.1f}"), FPS);

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

            // Recreate denoised HDR texture
            Desc.debugName = "DenoisedHDROutput";
            DenoisedHDRTexture = NvrhiDevice->createTexture(Desc);

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
        // Single command list: GBuffer -> GI -> Blit
        // =====================================================================
        nvrhi::CommandListParameters CmdListParams;
        CmdListParams.enableImmediateExecution = false;
        nvrhi::CommandListHandle CmdList = NvrhiDevice->createCommandList(CmdListParams);
        CmdList->open();

        // =====================================================================
        // Static camera setup - looking at Sponza
        // =====================================================================
        glm::vec3 CameraPos = SceneCenter + glm::vec3(0.0f, 2.0f, 8.0f);
        glm::mat4 view = glm::lookAtLH(CameraPos, SceneCenter, glm::vec3(0.0f, 1.0f, 0.0f));
        float aspectRatio = float(CurrentFBInfo.width) / float(CurrentFBInfo.height);
        glm::mat4 proj = glm::perspectiveLH_ZO(glm::radians(90.0f), aspectRatio, 0.1f, 1000.0f);

        // Write ViewConstants (column-major via glm::value_ptr)
        float ViewConstantsData[16 * 3 + 4]; // Model(16) + View(16) + Proj(16) + RenderTargetSize(2) + Padding(2)
        memcpy(&ViewConstantsData[0], glm::value_ptr(glm::scale(glm::mat4(1.0f), glm::vec3(2.0f))), 64);
        memcpy(&ViewConstantsData[16], glm::value_ptr(view), 64);
        memcpy(&ViewConstantsData[32], glm::value_ptr(proj), 64);
        ViewConstantsData[48] = static_cast<float>(CurrentFBInfo.width);
        ViewConstantsData[49] = static_cast<float>(CurrentFBInfo.height);
        ViewConstantsData[50] = 0.0f; // padding
        ViewConstantsData[51] = 0.0f; // padding
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
        // Transition GBuffer textures to ShaderResource for GI pass
        // =====================================================================
        CmdList->setTextureState(GBufferDiffuseTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(GBufferSpecularTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(GBufferNormalsTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(GBufferEmissiveTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(GBufferWorldPosTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(GBufferDepthTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

        // =====================================================================
        // GI Pass - Sequential Bounce Tracing
        // =====================================================================

        // Static light direction (stable for temporal accumulation testing)
        static const float LightAngle = 0.0f;
        glm::vec3 LightDir = glm::normalize(glm::vec3(sinf(LightAngle), 0.6f, cosf(LightAngle)));
        float GIConstantsData[16] = {
            LightDir.x, LightDir.y, LightDir.z, 1.0f,      // LightDir + intensity
            0.3f, 0.3f, 0.35f, 0.0f,                       // Ambient color (slightly blue)
            CameraPos.x, CameraPos.y, CameraPos.z, 1.0f,    // CameraPos (w=1.0)
            0.0f, 0.0f, 0.0f, 0.0f                         // padding
        };
        CmdList->writeBuffer(GIConstantBuffer, GIConstantsData, sizeof(GIConstantsData));

        // Transition HDR to UAV for GI write
        CmdList->setTextureState(HDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

        // Create GI binding set
        nvrhi::BindingSetDesc RTBindingSetDesc;
        RTBindingSetDesc.bindings = {
            nvrhi::BindingSetItem::ConstantBuffer(256, GIConstantBuffer),
            nvrhi::BindingSetItem::ConstantBuffer(512, ViewConstantsBuffer),
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
        // Denoise Pass: Bilateral filter on GI output
        // =====================================================================
        // Transition HDR (noisy) to ShaderResource for reading
        CmdList->setTextureState(HDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        // Transition DenoisedHDR to UAV for writing
        CmdList->setTextureState(DenoisedHDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
        // Transition depth to ShaderResource for reading
        CmdList->setTextureState(GBufferDepthTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

        FBilateralDenoisePass::FDesc DenoiseDesc;
        DenoiseDesc.InputTexture = HDRTexture;         // Noisy input
        DenoiseDesc.DepthTexture = GBufferDepthTexture; // Depth guide
        DenoiseDesc.NormalTexture = GBufferNormalsTexture; // Normal guide
        DenoiseDesc.OutputTexture = DenoisedHDRTexture; // Denoised output
        DenoiseDesc.OutputWidth = CurrentFBInfo.width;
        DenoiseDesc.OutputHeight = CurrentFBInfo.height;
        DenoiseDesc.DepthSigma = 0.01f;      // Sharp depth edges
        DenoiseDesc.NormalSigma = 0.1f;      // Sharp normal edges
        DenoiseDesc.SpatialSigma = 2.0f;    // 5x5 kernel effective radius
        DenoisePass.Dispatch(CmdList, DenoiseDesc);

        // =====================================================================
        // ReSTIR Generation Pass: Build per-pixel reservoirs
        // =====================================================================
        if (bReSTIRInitialized)
        {
            // Transition textures for ReSTIR
            CmdList->setTextureState(DenoisedHDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CmdList->setTextureState(GBufferWorldPosTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CmdList->setTextureState(GBufferNormalsTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CmdList->setTextureState(GBufferDepthTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CmdList->setTextureState(Reservoir0Texture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
            CmdList->setTextureState(Reservoir1Texture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
            CmdList->setTextureState(ReSTIRDebugTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

            ReSTIR::FReSTIRPass::FDesc ReSTIRDesc;
            ReSTIRDesc.RadianceTexture = DenoisedHDRTexture;
            ReSTIRDesc.WorldPosTexture = GBufferWorldPosTexture;
            ReSTIRDesc.NormalTexture = GBufferNormalsTexture;
            ReSTIRDesc.DepthTexture = GBufferDepthTexture;
            ReSTIRDesc.OutReservoir0 = Reservoir0Texture;
            ReSTIRDesc.OutReservoir1 = Reservoir1Texture;
            ReSTIRDesc.OutDebugTexture = ReSTIRDebugTexture;
            ReSTIRDesc.OutputWidth = CurrentFBInfo.width;
            ReSTIRDesc.OutputHeight = CurrentFBInfo.height;

            ReSTIR::FReSTIRConstants ReSTIRConstants;
            ReSTIRConstants.OutputSize[0] = static_cast<float>(CurrentFBInfo.width);
            ReSTIRConstants.OutputSize[1] = static_cast<float>(CurrentFBInfo.height);
            ReSTIRConstants.RcpOutputSize[0] = 1.0f / static_cast<float>(CurrentFBInfo.width);
            ReSTIRConstants.RcpOutputSize[1] = 1.0f / static_cast<float>(CurrentFBInfo.height);
            ReSTIRConstants.FrameIndex = static_cast<float>(TemporalFrameCount);
            ReSTIRConstants.DebugVis = g_ReSTIRDebugVis ? 1.0f : 0.0f;

            ReSTIRPass.Dispatch(CmdList, ReSTIRDesc, ReSTIRConstants);

            // =====================================================================
            // ReSTIR Temporal Reuse Pass: Merge with history
            // =====================================================================
            CmdList->setTextureState(Reservoir0Texture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CmdList->setTextureState(Reservoir1Texture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CmdList->setTextureState(Reservoir0HistoryTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CmdList->setTextureState(Reservoir1HistoryTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CmdList->setTextureState(GBufferDepthTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CmdList->setTextureState(Reservoir0MergedTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
            CmdList->setTextureState(Reservoir1MergedTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
            CmdList->setTextureState(ReSTIRDebugTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

            ReSTIR::FReSTIRPass::FTemporalDesc TemporalDesc;
            TemporalDesc.CurrentReservoir0 = Reservoir0Texture;
            TemporalDesc.CurrentReservoir1 = Reservoir1Texture;
            TemporalDesc.HistoryReservoir0 = Reservoir0HistoryTexture;
            TemporalDesc.HistoryReservoir1 = Reservoir1HistoryTexture;
            TemporalDesc.DepthTexture = GBufferDepthTexture;
            TemporalDesc.OutReservoir0 = Reservoir0MergedTexture;
            TemporalDesc.OutReservoir1 = Reservoir1MergedTexture;
            TemporalDesc.OutDebugTexture = ReSTIRDebugTexture;
            TemporalDesc.OutputWidth = CurrentFBInfo.width;
            TemporalDesc.OutputHeight = CurrentFBInfo.height;

            ReSTIR::FReSTIRTemporalConstants TemporalConstants;
            glm::mat4 currViewProj = proj * view;
            glm::mat4 invCurrViewProj = glm::inverse(currViewProj);
            memcpy(TemporalConstants.InverseCurrViewProj, glm::value_ptr(invCurrViewProj), sizeof(TemporalConstants.InverseCurrViewProj));
            memcpy(TemporalConstants.PrevViewProj, glm::value_ptr(PrevViewProj), sizeof(TemporalConstants.PrevViewProj));
            TemporalConstants.OutputSize[0] = static_cast<float>(CurrentFBInfo.width);
            TemporalConstants.OutputSize[1] = static_cast<float>(CurrentFBInfo.height);
            TemporalConstants.RcpOutputSize[0] = 1.0f / static_cast<float>(CurrentFBInfo.width);
            TemporalConstants.RcpOutputSize[1] = 1.0f / static_cast<float>(CurrentFBInfo.height);
            TemporalConstants.FrameIndex = static_cast<float>(TemporalFrameCount);
            TemporalConstants.MaxM = 30.0f;
            TemporalConstants.DebugVis = g_ReSTIRDebugVis ? 1.0f : 0.0f;

            ReSTIRPass.DispatchTemporal(CmdList, TemporalDesc, TemporalConstants);

            // Swap: merged output becomes next frame's history
            std::swap(Reservoir0MergedTexture, Reservoir0HistoryTexture);
            std::swap(Reservoir1MergedTexture, Reservoir1HistoryTexture);

            // =====================================================================
            // ReSTIR Spatial Reuse Pass: Merge with 3x3 neighbors
            // =====================================================================
            // After swap, Reservoir0/1HistoryTexture contains the merged result
            CmdList->setTextureState(Reservoir0HistoryTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CmdList->setTextureState(Reservoir1HistoryTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CmdList->setTextureState(GBufferNormalsTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CmdList->setTextureState(GBufferDepthTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CmdList->setTextureState(DenoisedHDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CmdList->setTextureState(SpatialRadianceTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
            CmdList->setTextureState(ReSTIRDebugTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

            ReSTIR::FReSTIRPass::FSpatialDesc SpatialDesc;
            SpatialDesc.MergedReservoir0 = Reservoir0HistoryTexture;
            SpatialDesc.MergedReservoir1 = Reservoir1HistoryTexture;
            SpatialDesc.NormalTexture = GBufferNormalsTexture;
            SpatialDesc.DepthTexture = GBufferDepthTexture;
            SpatialDesc.RadianceTexture = DenoisedHDRTexture;
            SpatialDesc.OutRadiance = SpatialRadianceTexture;
            SpatialDesc.OutDebugTexture = ReSTIRDebugTexture;
            SpatialDesc.OutputWidth = CurrentFBInfo.width;
            SpatialDesc.OutputHeight = CurrentFBInfo.height;

            ReSTIR::FReSTIRSpatialConstants SpatialConstants;
            SpatialConstants.OutputSize[0] = static_cast<float>(CurrentFBInfo.width);
            SpatialConstants.OutputSize[1] = static_cast<float>(CurrentFBInfo.height);
            SpatialConstants.RcpOutputSize[0] = 1.0f / static_cast<float>(CurrentFBInfo.width);
            SpatialConstants.RcpOutputSize[1] = 1.0f / static_cast<float>(CurrentFBInfo.height);
            SpatialConstants.NormalSigma = 0.1f;
            SpatialConstants.PlaneSigma = 100.0f;
            SpatialConstants.DepthSigma = 0.01f;
            SpatialConstants.MaxM = 30.0f;
            SpatialConstants.SpatialRadius = 3.0f;
            SpatialConstants.DebugVis = g_ReSTIRDebugVis ? 2.0f : 0.0f;

            ReSTIRPass.DispatchSpatial(CmdList, SpatialDesc, SpatialConstants);

            // Update PrevViewProj for next frame
            PrevViewProj = currViewProj;
        }

        // =====================================================================
        // ReBLUR Pass: Temporal denoiser with SH encoding
        // =====================================================================
        if (bReBLURInitialized)
        {
            // Compute current ViewProj and its inverse for ReBLUR
            glm::mat4 currViewProj = proj * view;
            glm::mat4 invCurrViewProj = glm::inverse(currViewProj);

            // Transition textures for ReBLUR
            nvrhi::TextureHandle ReBLURInputTexture = bReSTIRInitialized ? SpatialRadianceTexture : DenoisedHDRTexture;
            CmdList->setTextureState(ReBLURInputTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CmdList->setTextureState(ReBLURHistoryTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CmdList->setTextureState(ReBLUROutputTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
            CmdList->setTextureState(GBufferDepthTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CmdList->setTextureState(GBufferNormalsTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

            ReBLUR::FReBLURPass::FDesc ReBLURDesc;
            ReBLURDesc.CurrentRadianceTexture = ReBLURInputTexture; // RGB = radiance, A = confidence (from spatial reuse)
            ReBLURDesc.HistoryTexture = ReBLURHistoryTexture;
            ReBLURDesc.DepthTexture = GBufferDepthTexture;
            ReBLURDesc.NormalRoughnessTexture = GBufferNormalsTexture; // Note: normals only, roughness from GBuffer
            ReBLURDesc.OutputTexture = ReBLUROutputTexture;
            ReBLURDesc.OutputWidth = CurrentFBInfo.width;
            ReBLURDesc.OutputHeight = CurrentFBInfo.height;

            ReBLUR::FReBLURConstants ReBLURConstants;
            memcpy(ReBLURConstants.InverseCurrViewProj, glm::value_ptr(invCurrViewProj), sizeof(ReBLURConstants.InverseCurrViewProj));
            memcpy(ReBLURConstants.PrevViewProj, glm::value_ptr(PrevViewProj), sizeof(ReBLURConstants.PrevViewProj));
            memcpy(ReBLURConstants.ViewMatrix, glm::value_ptr(view), sizeof(ReBLURConstants.ViewMatrix));
            memcpy(ReBLURConstants.ProjMatrix, glm::value_ptr(proj), sizeof(ReBLURConstants.ProjMatrix));
            ReBLURConstants.OutputSize[0] = static_cast<float>(CurrentFBInfo.width);
            ReBLURConstants.OutputSize[1] = static_cast<float>(CurrentFBInfo.height);
            ReBLURConstants.RcpOutputSize[0] = 1.0f / static_cast<float>(CurrentFBInfo.width);
            ReBLURConstants.RcpOutputSize[1] = 1.0f / static_cast<float>(CurrentFBInfo.height);
            // Hit distance normalization params (A, B, C, D)
            ReBLURConstants.HitDistParams[0] = 3.0f;
            ReBLURConstants.HitDistParams[1] = 0.1f;
            ReBLURConstants.HitDistParams[2] = 20.0f;
            ReBLURConstants.HitDistParams[3] = -25.0f;
            ReBLURConstants.FrameIndex = static_cast<float>(TemporalFrameCount);
            ReBLURConstants.HistoryFadeIn = 6.0f;
            ReBLURConstants.ConfidenceScale = 1.0f;

            ReBLUR::FPooledBlurParams BlurParams;
            BlurParams.BlurRadius = 0.0f;  // Spatial blur disabled for now
            BlurParams.NormalWeight = 0.1f;
            BlurParams.PlaneWeight = 2.0f;
            BlurParams.RoughnessWeight = 0.3f;
            BlurParams.AntiLagIntensity = 0.5f;
            BlurParams.DarknessSensitivity = 0.01f;

            // Single dispatch: temporal accumulation only
            ReBLURPass.Dispatch(CmdList, ReBLURDesc, ReBLURConstants, BlurParams);

            // Ping-pong swap for next frame
            std::swap(ReBLURHistoryTexture, ReBLUROutputTexture);

            // Update PrevViewProj for next frame
            PrevViewProj = currViewProj;
        }

        // =====================================================================
        // Frame dump: copy ReBLUR output to staging (before blit, on same command list)
        // =====================================================================
        static bool dumpRequested = !!getenv("HLVM_DUMP_GI");
        static int framesToDump = 0;
        bool doDumpThisFrame = dumpRequested && framesToDump < 4;
        nvrhi::TextureHandle DumpTexture;
        // Frame dump logic
        static bool bDebugDumpSpatial = false;
        if (bDebugDumpSpatial && bReSTIRInitialized)
        {
            DumpTexture = SpatialRadianceTexture;
        }
        else if (g_ReSTIRDebugVis && bReSTIRInitialized)
        {
            DumpTexture = ReSTIRDebugTexture;
        }
        else if (bReBLURInitialized)
        {
            DumpTexture = ReBLURHistoryTexture;
        }
        else
        {
            DumpTexture = bReSTIRInitialized ? SpatialRadianceTexture : DenoisedHDRTexture;
        }
        if (doDumpThisFrame) {
            framesToDump++;
            CmdList->setTextureState(DumpTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopySource);
            nvrhi::TextureSlice slice = {};
            slice.width = CurrentFBInfo.width;
            slice.height = CurrentFBInfo.height;
            slice.depth = 1;
            CmdList->copyTexture(StagingTexture.Get(), slice, DumpTexture.Get(), slice);
        }

        // =====================================================================
        // Always blit ReBLUR output to screen (or denoised HDR if ReBLUR disabled)
        // =====================================================================
        CmdList->setTextureState(DumpTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        FCommonRenderPasses::BlitParameters BlitParams;
        FCommonRenderPasses::BlitTexture(
            CmdList,
            Framebuffer,
            DumpTexture,
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
                FString DumpDir = FString::Format(TXT("{}/Engine/Source/Runtime/Test/TestFewBounceGI_Data/dumps"), *GProjectRoot);
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
        DenoisedHDRTexture = nullptr;
        ReBLURHistoryTexture = nullptr;
        ReBLUROutputTexture = nullptr;
        Reservoir0Texture = nullptr;
        Reservoir1Texture = nullptr;
        Reservoir0HistoryTexture = nullptr;
        Reservoir1HistoryTexture = nullptr;
        Reservoir0MergedTexture = nullptr;
        Reservoir1MergedTexture = nullptr;
        SpatialRadianceTexture = nullptr;
        ReSTIRDebugTexture = nullptr;
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

    // GI shaders
    nvrhi::ShaderLibraryHandle ShaderLibrary;
    nvrhi::ShaderHandle        RayGenShader;
    nvrhi::ShaderHandle        ClosestHitShader;
    nvrhi::ShaderHandle        MissShader;

    // Acceleration structures
    nvrhi::rt::AccelStructHandle          TopLevelAS;
    TVector<nvrhi::rt::AccelStructHandle> MeshBLASHandles;
    FTLASBuilder                          TLASBuilder;

    // GI pipeline
    nvrhi::rt::PipelineHandle    RTPipeline;
    nvrhi::rt::ShaderTableHandle ShaderTable;
    nvrhi::BindingLayoutHandle   RTBindingLayout;
    nvrhi::BindingSetHandle      RTBindingSet;
    nvrhi::TextureHandle         HDRTexture;
    nvrhi::TextureHandle         DenoisedHDRTexture;  // Denoised output

    // Constant buffer for GI
    nvrhi::BufferHandle GIConstantBuffer;

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

    // Denoise pass (pre-denoiser before ReBLUR)
    FBilateralDenoisePass DenoisePass;

    // ReBLUR pass (temporal denoiser)
    ReBLUR::FReBLURPass ReBLURPass;
    nvrhi::TextureHandle ReBLURHistoryTexture;  // Ping-pong history (SH encoded)
    nvrhi::TextureHandle ReBLUROutputTexture;  // Ping-pong output
    bool bReBLURInitialized = false;
    glm::mat4 PrevViewProj = glm::mat4(1.0f);  // Previous frame's view-projection for ReBLUR

    // ReSTIR pass (reservoir generation + temporal + spatial reuse)
    ReSTIR::FReSTIRPass ReSTIRPass;
    nvrhi::TextureHandle Reservoir0Texture;
    nvrhi::TextureHandle Reservoir1Texture;
    nvrhi::TextureHandle Reservoir0HistoryTexture;
    nvrhi::TextureHandle Reservoir1HistoryTexture;
    nvrhi::TextureHandle Reservoir0MergedTexture;
    nvrhi::TextureHandle Reservoir1MergedTexture;
    nvrhi::TextureHandle SpatialRadianceTexture;  // Phase 8 output
    nvrhi::TextureHandle ReSTIRDebugTexture;
    bool bReSTIRInitialized = false;

    // Staging texture for frame dumps
    nvrhi::StagingTextureHandle StagingTexture;

    uint32_t LastWidth = 0;
    uint32_t LastHeight = 0;
    uint32_t FrameCount = 0;
    uint32_t TemporalFrameCount = 0;
    float    FPSUpdateTimer = 0.0f;
};

// =============================================================================
// TEST IMPLEMENTATION
// =============================================================================

RECORD_BOOL(test_FewBounceGI)
{
    HLVM_LOG(LogTest, info, TXT("=== Starting Few-bounce GI Test ==="));

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
        // Suppress known validation layer false positive
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
        TSharedPtr<FFewBounceGIPass> GIPass =
            std::make_shared<FFewBounceGIPass>(DeviceManager.get());
        if (!GIPass->Initialize(NvrhiDevice, FirstFB, FString(TXT("Few-bounce GI Test"))))
        {
            throw std::runtime_error("Failed to initialize FFewBounceGIPass");
        }

        DeviceManager->AddRenderPassToBack(GIPass);

        HLVM_LOG(LogTest, info, TXT("Starting render loop..."));

        std::thread([&]() {
            FTimer Timer;
            while (Timer.MarkSec() < 5.0)
            {
            }
            DeviceManager->StopMessageLoop();
        }).detach();

        DeviceManager->RunMessageLoop();

        GIPass->Shutdown();

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

RECORD_BOOL(test_FewBounceGI)
{
    HLVM_LOG(LogTest, warning, TXT("Vulkan renderer not enabled - skipping test"));
    return true;
}

#endif // HLVM_VULKAN_RENDERER
