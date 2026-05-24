/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * TestRenderSponza - Forward Rendering with KTX2 Textured Sponza
 *
 * Pipeline:
 * 1. Load Sponza scene (sponza/Sponza01.gltf) with KTX2 texture redirection
 * 2. Forward Pass: Render all 27 meshes with per-mesh diffuse textures
 * 3. Static camera placed inside the courtyard
 */

#include "Test.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/RHI/Object/Buffer.h"
#include "Renderer/Common/FBindingCache.h"
#include "Renderer/SceneGraph/FNode.h"
#include "Renderer/Scene3D/Scene3DLoader.h"
#include "Renderer/Mesh/StaticMesh.h"
#include "Renderer/Material/PBRMaterial.h"
#include "Renderer/Texture/AsyncTextureLoader.h"
#include "Renderer/Texture/TextureCache.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
#include "Image/FImageDump.h"
#include "Core/Parallel/Async/WorkStealThreadPool.h"
#include <nvrhi/utils.h>
#include <Utility/Timer.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>

DECLARE_LOG_CATEGORY(LogTest)

#if HLVM_VULKAN_RENDERER

// =============================================================================
// CONFIGURATION
// =============================================================================

static const char*    WINDOW_TITLE = "Render Sponza - Forward";
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
// FRenderSponzaPass
// =============================================================================

class FRenderSponzaPass : public IRenderPass
{
public:
    using IRenderPass::IRenderPass;

    bool Initialize(nvrhi::IDevice* Device, nvrhi::IFramebuffer* Framebuffer, const FString& InWindowTitle)
    {
        HLVM_LOG(LogTest, info, TXT("=== FRenderSponzaPass::Initialize ==="));

        NvrhiDevice = Device;
        FBInfo = Framebuffer->getFramebufferInfo();
        WindowTitle = InWindowTitle;

        BindingCache.SetDevice(NvrhiDevice);

        const auto DataDir = FString::Format(
            TXT("{}/Engine/Source/Runtime/Test/TestRenderSponza_Data"),
            *GProjectRoot);

        // =====================================================================
        // Load shaders from ShaderMake blob
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Loading shaders..."));

        auto VSBlob = ReadBinaryFile(
            FPath::Combine(DataDir, TXT("SponzaForwardVS.sblob")).string());
        const void* VSBinary = nullptr;
        size_t      VSBinarySize = 0;
        if (!ShaderMake::FindPermutationInBlob(VSBlob.data(), VSBlob.size(), nullptr, 0, &VSBinary, &VSBinarySize))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to extract VS from blob"));
            return false;
        }

        auto PSBlob = ReadBinaryFile(
            FPath::Combine(DataDir, TXT("SponzaForwardPS.sblob")).string());
        const void* PSBinary = nullptr;
        size_t      PSBinarySize = 0;
        if (!ShaderMake::FindPermutationInBlob(PSBlob.data(), PSBlob.size(), nullptr, 0, &PSBinary, &PSBinarySize))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to extract PS from blob"));
            return false;
        }

        nvrhi::ShaderDesc VSDesc;
        VSDesc.setShaderType(nvrhi::ShaderType::Vertex);
        VS = NvrhiDevice->createShader(VSDesc, VSBinary, VSBinarySize);

        nvrhi::ShaderDesc PSDesc;
        PSDesc.setShaderType(nvrhi::ShaderType::Pixel);
        PS = NvrhiDevice->createShader(PSDesc, PSBinary, PSBinarySize);

        if (!VS || !PS)
        {
            HLVM_LOG(LogTest, err, TXT("Failed to create shaders"));
            return false;
        }
        HLVM_LOG(LogTest, info, TXT("Shaders loaded successfully"));

        // =====================================================================
        // Load Sponza scene
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Loading Sponza scene..."));
        const FString GitRoot = FString::Format(TXT("{}"), *GProjectRoot);
        const FPath   ScenePath = FPath(FString::Format(
              TXT("{}/Samples/Assets/sponza/Sponza01.gltf"), *GitRoot));

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
        SceneCenter = (BBoxMin + BBoxMax) * 0.5f;
        float SceneRadius = glm::length(BBoxMax - BBoxMin) * 0.5f;
        HLVM_LOG(LogTest, info, TXT("Scene center: ({:.2f}, {:.2f}, {:.2f}), radius: {:.2f}"),
            SceneCenter.x, SceneCenter.y, SceneCenter.z, SceneRadius);

        // =====================================================================
        // Load PBR textures for materials (async decode + batched GPU upload)
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Loading PBR textures..."));

        FAsyncTextureLoader::LoadMaterialTexturesAsync(
            NvrhiDevice, Materials,
            {IMaterial::ETextureType::Albedo});
        NvrhiDevice->waitForIdle();

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
        // Create input layout (uses offsetof)
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

            InputLayout = NvrhiDevice->createInputLayout(Attrs, 4, VS);
        }

        // =====================================================================
        // Create binding layout
        // =====================================================================
        {
            nvrhi::BindingLayoutDesc LayoutDesc;
            LayoutDesc.visibility = nvrhi::ShaderType::Vertex | nvrhi::ShaderType::Pixel;

            nvrhi::VulkanBindingOffsets offsets;
            offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
            LayoutDesc.setBindingOffsets(offsets);

            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::ConstantBuffer(256), // b0: MVP -> 256 (bRegShift)
                nvrhi::BindingLayoutItem::ConstantBuffer(257), // b1: Light -> 257
                nvrhi::BindingLayoutItem::Texture_SRV(0),      // t0: Diffuse -> 0 (tRegShift)
                nvrhi::BindingLayoutItem::Sampler(128)         // s0: LinearSampler -> 128 (sRegShift)
            };

            BindingLayout = NvrhiDevice->createBindingLayout(LayoutDesc);
        }

        // =====================================================================
        // Create constant buffers
        // =====================================================================
        {
            nvrhi::BufferDesc CBDesc;
            CBDesc.byteSize = sizeof(float) * 16 * 3; // Model + View + Proj
            CBDesc.isConstantBuffer = true;
            CBDesc.isVolatile = false;
            CBDesc.keepInitialState = true;
            CBDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
            CBDesc.debugName = "MVPConstants";
            MVPConstantsBuffer = NvrhiDevice->createBuffer(CBDesc);
        }

        {
            nvrhi::BufferDesc CBDesc;
            CBDesc.byteSize = sizeof(float) * 8; // LightPos(3+1) + CameraPos(3+1)
            CBDesc.isConstantBuffer = true;
            CBDesc.isVolatile = false;
            CBDesc.keepInitialState = true;
            CBDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
            CBDesc.debugName = "LightConstants";
            LightConstantsBuffer = NvrhiDevice->createBuffer(CBDesc);
        }

        // =====================================================================
        // Create sampler
        // =====================================================================
        {
            nvrhi::SamplerDesc SamplerDesc;
            SamplerDesc.setAddressU(nvrhi::SamplerAddressMode::Repeat)
                .setAddressV(nvrhi::SamplerAddressMode::Repeat)
                .setAddressW(nvrhi::SamplerAddressMode::Repeat)
                .setMinFilter(true)
                .setMagFilter(true)
                .setMipFilter(true);
            LinearSampler = NvrhiDevice->createSampler(SamplerDesc);
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
        // Create graphics pipeline
        // =====================================================================
        {
            nvrhi::GraphicsPipelineDesc PipelineDesc;
            PipelineDesc.setVertexShader(VS);
            PipelineDesc.setPixelShader(PS);
            PipelineDesc.setInputLayout(InputLayout);
            PipelineDesc.addBindingLayout(BindingLayout);
            PipelineDesc.setPrimType(nvrhi::PrimitiveType::TriangleList);
            PipelineDesc.renderState.setRasterState(nvrhi::RasterState().setCullNone());
            PipelineDesc.renderState.depthStencilState
                .setDepthTestEnable(true)
                .setDepthWriteEnable(true)
                .setDepthFunc(nvrhi::ComparisonFunc::Less);

            Pipeline = NvrhiDevice->createGraphicsPipeline(PipelineDesc, FBInfo);
            if (!Pipeline)
            {
                HLVM_LOG(LogTest, err, TXT("Failed to create graphics pipeline"));
                return false;
            }
        }
        HLVM_LOG(LogTest, info, TXT("Graphics pipeline created"));

        HLVM_LOG(LogTest, info, TXT("FRenderSponzaPass initialized successfully"));
        return true;
    }

    void Shutdown()
    {
        HLVM_LOG(LogTest, info, TXT("FRenderSponzaPass::Shutdown"));

        BindingCache.Clear();

        // Clear texture cache to prevent VUID-vkDestroyDevice-device-05137
        FTextureCache::Get().Clear();

        VS = nullptr;
        PS = nullptr;
        InputLayout = nullptr;
        BindingLayout = nullptr;
        Pipeline = nullptr;
        MVPConstantsBuffer = nullptr;
        LightConstantsBuffer = nullptr;
        LinearSampler = nullptr;
        PlaceholderTexture = nullptr;

        AllMeshDrawData.clear();
    }

    virtual void Animate(float fElapsedTimeSeconds) override
    {
        FrameCount++;
        FPSUpdateTimer += fElapsedTimeSeconds;
        float FPS = float(FrameCount) / FPSUpdateTimer;
        if (FPSUpdateTimer >= 1.0f)
        {
            WindowTitle = FString::Format(TXT("Render Sponza - FPS: {:.1f}"), FPS);

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
        // Recreate pipeline if framebuffer info changed (resize)
        // =====================================================================
        if (CurrentFBInfo.width != LastWidth || CurrentFBInfo.height != LastHeight)
        {
            LastWidth = CurrentFBInfo.width;
            LastHeight = CurrentFBInfo.height;

            nvrhi::GraphicsPipelineDesc PipelineDesc;
            PipelineDesc.setVertexShader(VS);
            PipelineDesc.setPixelShader(PS);
            PipelineDesc.setInputLayout(InputLayout);
            PipelineDesc.addBindingLayout(BindingLayout);
            PipelineDesc.setPrimType(nvrhi::PrimitiveType::TriangleList);
            PipelineDesc.renderState.setRasterState(nvrhi::RasterState().setCullNone());
            PipelineDesc.renderState.depthStencilState
                .setDepthTestEnable(true)
                .setDepthWriteEnable(true)
                .setDepthFunc(nvrhi::ComparisonFunc::Less);

            Pipeline = NvrhiDevice->createGraphicsPipeline(PipelineDesc, CurrentFBInfo);
            BindingCache.Clear();
        }

        // =====================================================================
        // Static camera inside the courtyard
        // =====================================================================
        glm::vec3 CameraPos = SceneCenter + glm::vec3(0.0f, 2.0f, 0.0f);
        glm::vec3 LookAt = SceneCenter + glm::vec3(0.0f, 2.0f, -10.0f);
        glm::mat4 view = glm::lookAtLH(CameraPos, LookAt, glm::vec3(0.0f, 1.0f, 0.0f));
        float aspectRatio = float(CurrentFBInfo.width) / float(CurrentFBInfo.height);
        glm::mat4 proj = glm::perspectiveLH_ZO(glm::radians(90.0f), aspectRatio, 0.1f, 1000.0f);

        // =====================================================================
        // Single command list for forward pass
        // =====================================================================
        nvrhi::CommandListParameters CmdListParams;
        CmdListParams.enableImmediateExecution = false;
        nvrhi::CommandListHandle CmdList = NvrhiDevice->createCommandList(CmdListParams);
        CmdList->open();

        // Write MVP constants (column-major via glm::value_ptr)
        float MVPData[16 * 3];
        memcpy(&MVPData[0], glm::value_ptr(glm::scale(glm::mat4(1.0f), glm::vec3(1.0f))), 64);
        memcpy(&MVPData[16], glm::value_ptr(view), 64);
        memcpy(&MVPData[32], glm::value_ptr(proj), 64);
        CmdList->writeBuffer(MVPConstantsBuffer, MVPData, sizeof(MVPData));

        // Write light constants
        float LightData[8] = {
            SceneCenter.x + 5.0f, SceneCenter.y + 10.0f, SceneCenter.z + 5.0f, 0.0f,  // LightPosition
            CameraPos.x, CameraPos.y, CameraPos.z, 0.0f                                 // CameraPosition
        };
        CmdList->writeBuffer(LightConstantsBuffer, LightData, sizeof(LightData));

        // Clear attachments
        nvrhi::Color ClearColor(0.1f, 0.15f, 0.3f, 1.0f);
        nvrhi::utils::ClearColorAttachment(CmdList, Framebuffer, 0, ClearColor);
        nvrhi::utils::ClearDepthStencilAttachment(CmdList, Framebuffer, 1.0f, 0u);

        // =====================================================================
        // Draw all meshes with per-mesh material texture binding
        // =====================================================================
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

            // Create binding set for this mesh
            nvrhi::BindingSetDesc BindingSetDesc;
            BindingSetDesc.bindings = {
                nvrhi::BindingSetItem::ConstantBuffer(256, MVPConstantsBuffer),
                nvrhi::BindingSetItem::ConstantBuffer(257, LightConstantsBuffer),
                nvrhi::BindingSetItem::Texture_SRV(0, DiffuseTex),
                nvrhi::BindingSetItem::Sampler(128, LinearSampler)
            };
            nvrhi::BindingSetHandle BindingSet = BindingCache.GetOrCreateBindingSet(BindingSetDesc, BindingLayout);

            // Build graphics state
            nvrhi::GraphicsState State;
            State.setPipeline(Pipeline);
            State.setFramebuffer(Framebuffer);
            State.addBindingSet(BindingSet);

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
            HLVM_LOG(LogTest, info, TXT("Forward Pass: drew {} meshes"), AllMeshDrawData.size());
        }

        CmdList->close();
        NvrhiDevice->executeCommandList(CmdList);
    }

    virtual void BackBufferResizing() override
    {
        Pipeline = nullptr;
        BindingCache.Clear();
    }

private:
    nvrhi::IDevice*        NvrhiDevice = nullptr;
    nvrhi::FramebufferInfo FBInfo;
    FString                WindowTitle;

    // Scene
    std::shared_ptr<FScene3DNode> Scene;
    glm::vec3 SceneCenter = glm::vec3(0.f);

    // Shaders
    nvrhi::ShaderHandle VS;
    nvrhi::ShaderHandle PS;

    // Pipeline
    nvrhi::InputLayoutHandle      InputLayout;
    nvrhi::BindingLayoutHandle    BindingLayout;
    nvrhi::GraphicsPipelineHandle Pipeline;

    // Constant buffers
    nvrhi::BufferHandle MVPConstantsBuffer;
    nvrhi::BufferHandle LightConstantsBuffer;

    // Sampler
    nvrhi::SamplerHandle LinearSampler;

    // Placeholder texture
    nvrhi::TextureHandle PlaceholderTexture;

    // Per-mesh data
    struct FMeshDrawData {
        nvrhi::BufferHandle VertexBuffer;
        nvrhi::BufferHandle IndexBuffer;
        std::shared_ptr<FStaticMesh> Mesh;
        uint32_t IndexCount;
    };
    TVector<FMeshDrawData> AllMeshDrawData;

    FBindingCache BindingCache;

    uint32_t LastWidth = 0;
    uint32_t LastHeight = 0;
    uint32_t FrameCount = 0;
    float    FPSUpdateTimer = 0.0f;
};

// =============================================================================
// TEST IMPLEMENTATION
// =============================================================================

RECORD_BOOL(test_RenderSponza)
{
    HLVM_LOG(LogTest, info, TXT("=== Starting Render Sponza Test ==="));

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
        DeviceParams.bEnableRayTracingExtensions = false;

        if (!DeviceManager->CreateWindowDeviceAndSwapChain(WindowProps))
        {
            throw std::runtime_error("Failed to create window, device and swap chain");
        }

        nvrhi::IDevice*      NvrhiDevice = DeviceManager->GetDevice();
        nvrhi::IFramebuffer* FirstFB = DeviceManager->GetFramebuffer(0);

        HLVM_LOG(LogTest, info, TXT("Creating render pass..."));
        TSharedPtr<FRenderSponzaPass> RenderPass =
            std::make_shared<FRenderSponzaPass>(DeviceManager.get());
        if (!RenderPass->Initialize(NvrhiDevice, FirstFB, FString(TXT("Render Sponza"))))
        {
            throw std::runtime_error("Failed to initialize FRenderSponzaPass");
        }

        DeviceManager->AddRenderPassToBack(RenderPass);

        HLVM_LOG(LogTest, info, TXT("Starting render loop..."));

        std::thread([&]() {
            FTimer Timer;
            while (Timer.MarkSec() < 3.0)
            {
            }
            DeviceManager->StopMessageLoop();
        }).detach();

        DeviceManager->RunMessageLoop();

        RenderPass->Shutdown();

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

RECORD_BOOL(test_RenderSponza)
{
    HLVM_LOG(LogTest, warning, TXT("Vulkan renderer not enabled - skipping test"));
    return true;
}

#endif // HLVM_VULKAN_RENDERER
