/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * TestPBRLighting - Cook-Torrance BRDF validation
 *
 * Renders a procedural sphere with material bands:
 *   Top: Gold (metallic, smooth) - should show sharp specular highlight
 *   Middle: Red plastic (dielectric, medium roughness)
 *   Bottom: Blue rough (dielectric, rough) - should be diffuse only
 */

#include "Test.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Common/FBindingCache.h"
#include "Renderer/Common/FCommonRenderPasses.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
#include "Image/FRenderPassDumper.h"
#include <nvrhi/utils.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

DECLARE_LOG_CATEGORY(LogTest)

#if HLVM_VULKAN_RENDERER

// =============================================================================
// CONFIGURATION
// =============================================================================

static const char*    WINDOW_TITLE = "PBR Lighting Test";
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
// FPBRLightingPass
// =============================================================================

class FPBRLightingPass : public IRenderPass
{
public:
    using IRenderPass::IRenderPass;

    bool Initialize(nvrhi::IDevice* Device, nvrhi::IFramebuffer* Framebuffer, const FString& InWindowTitle)
    {
        HLVM_LOG(LogTest, info, TXT("=== FPBRLightingPass::Initialize ==="));

        NvrhiDevice = Device;
        FBInfo = Framebuffer->getFramebufferInfo();
        WindowTitle = InWindowTitle;

        BindingCache.SetDevice(NvrhiDevice);

        const auto ShaderDataDir = FString::Format(
            TXT("{}/Engine/Source/Runtime/Test/TestPBRLighting_Data"),
            *GProjectRoot);

        // =====================================================================
        // Load compute shader
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Loading PBR lighting shader..."));

        auto CSBlob = ReadBinaryFile(
            FPath::Combine(ShaderDataDir, TXT("PBRLighting_cs.sblob")).string());
        const void* CSBinary = nullptr;
        size_t      CSBinarySize = 0;
        if (!ShaderMake::FindPermutationInBlob(CSBlob.data(), CSBlob.size(), nullptr, 0, &CSBinary, &CSBinarySize))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to extract PBRLighting_cs from blob"));
            return false;
        }

        nvrhi::ShaderDesc CSDesc;
        CSDesc.setShaderType(nvrhi::ShaderType::Compute);
        LightingCS = NvrhiDevice->createShader(CSDesc, CSBinary, CSBinarySize);

        if (!LightingCS)
        {
            HLVM_LOG(LogTest, err, TXT("Failed to create lighting compute shader"));
            return false;
        }
        HLVM_LOG(LogTest, info, TXT("PBR lighting shader loaded successfully"));

        // =====================================================================
        // Create HDR output texture
        // =====================================================================
        {
            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = WINDOW_WIDTH;
            Desc.height = WINDOW_HEIGHT;
            Desc.format = nvrhi::Format::RGBA32_FLOAT;
            Desc.isRenderTarget = false;
            Desc.isUAV = true;
            Desc.isTypeless = false;
            Desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
            Desc.keepInitialState = true;
            Desc.debugName = "PBRLighting_HDROutput";
            HDROutputTexture = NvrhiDevice->createTexture(Desc);
        }

        // =====================================================================
        // Create constant buffer
        // =====================================================================
        {
            nvrhi::BufferDesc CBDesc;
            CBDesc.byteSize = sizeof(float) * 16; // 4 float4s = 64 bytes
            CBDesc.isConstantBuffer = true;
            CBDesc.isVolatile = false;
            CBDesc.keepInitialState = true;
            CBDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
            CBDesc.debugName = "LightingConstants";
            ConstantBuffer = NvrhiDevice->createBuffer(CBDesc);
        }

        // =====================================================================
        // Create binding layout
        // =====================================================================
        {
            nvrhi::BindingLayoutDesc LayoutDesc;
            LayoutDesc.visibility = nvrhi::ShaderType::Compute;

            nvrhi::VulkanBindingOffsets offsets;
            offsets.setConstantBufferOffset(0)
                   .setShaderResourceOffset(0)
                   .setSamplerOffset(0)
                   .setUnorderedAccessViewOffset(0);
            LayoutDesc.setBindingOffsets(offsets);

            LayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(256));
            LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_UAV(384));

            BindingLayout = NvrhiDevice->createBindingLayout(LayoutDesc);
        }

        // =====================================================================
        // Create compute pipeline
        // =====================================================================
        {
            nvrhi::ComputePipelineDesc PipelineDesc;
            PipelineDesc.setComputeShader(LightingCS);
            PipelineDesc.addBindingLayout(BindingLayout);

            ComputePipeline = NvrhiDevice->createComputePipeline(PipelineDesc);
            if (!ComputePipeline)
            {
                HLVM_LOG(LogTest, err, TXT("Failed to create compute pipeline"));
                return false;
            }
        }
        HLVM_LOG(LogTest, info, TXT("Compute pipeline created"));

        // =====================================================================
        // Initialize frame dumper
        // =====================================================================
        FrameDumper.Initialize(NvrhiDevice, nvrhi::Format::RGBA32_FLOAT);
        FrameDumper.SetTestName(TXT("TestPBRLighting"));

        HLVM_LOG(LogTest, info, TXT("FPBRLightingPass initialized successfully"));
        return true;
    }

    void Shutdown()
    {
        HLVM_LOG(LogTest, info, TXT("FPBRLightingPass::Shutdown"));

        BindingCache.Clear();

        ComputePipeline = nullptr;
        BindingLayout = nullptr;
        LightingCS = nullptr;
        HDROutputTexture = nullptr;
        ConstantBuffer = nullptr;
    }

    virtual void Animate(float fElapsedTimeSeconds) override
    {
        FrameCount++;
        FPSUpdateTimer += fElapsedTimeSeconds;
        float FPS = float(FrameCount) / FPSUpdateTimer;
        if (FPSUpdateTimer >= 1.0f)
        {
            WindowTitle = FString::Format(TXT("PBR Lighting - FPS: {:.1f}"), FPS);

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
        // Resize handling
        // =====================================================================
        if (!HDROutputTexture || CurrentFBInfo.width != LastWidth || CurrentFBInfo.height != LastHeight)
        {
            LastWidth = CurrentFBInfo.width;
            LastHeight = CurrentFBInfo.height;

            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = CurrentFBInfo.width;
            Desc.height = CurrentFBInfo.height;
            Desc.format = nvrhi::Format::RGBA32_FLOAT;
            Desc.isRenderTarget = false;
            Desc.isUAV = true;
            Desc.isTypeless = false;
            Desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
            Desc.keepInitialState = true;
            Desc.debugName = "PBRLighting_HDROutput";
            HDROutputTexture = NvrhiDevice->createTexture(Desc);

            BindingCache.Clear();
        }

        // =====================================================================
        // Compute Pass
        // =====================================================================
        nvrhi::CommandListParameters CmdListParams;
        CmdListParams.enableImmediateExecution = false;
        nvrhi::CommandListHandle CmdList = NvrhiDevice->createCommandList(CmdListParams);
        CmdList->open();

        // Write lighting constants
        // Light from upper-left-back, shining towards lower-right-front
        // This lights the front face and top of the sphere
        // HLSL packs float3 + float into one 16-byte register.
        // Layout: float3 LightDir; float LightIntensity;
        //         float3 CameraPos; float Pad0;
        //         float3 AmbientColor; float Pad1;
        float LightingData[12] = {
            0.3f, -0.6f, 0.7f, 2.0f,   // LightDir.xyz + LightIntensity (from below to light gold band)
            0.0f, 0.0f, 2.5f, 0.0f,    // CameraPos.xyz + Pad0
            0.05f, 0.05f, 0.05f, 0.0f  // AmbientColor.xyz + Pad1
        };
        CmdList->writeBuffer(ConstantBuffer, LightingData, sizeof(LightingData));

        // Transition HDR to UAV
        CmdList->setTextureState(HDROutputTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

        // Create binding set
        nvrhi::BindingSetDesc SetDesc;
        SetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(256, ConstantBuffer));
        SetDesc.addItem(nvrhi::BindingSetItem::Texture_UAV(384, HDROutputTexture));
        nvrhi::BindingSetHandle Bindings = BindingCache.GetOrCreateBindingSet(SetDesc, BindingLayout);

        // Dispatch
        uint32_t dispatchX = (CurrentFBInfo.width + 7) / 8;
        uint32_t dispatchY = (CurrentFBInfo.height + 7) / 8;

        nvrhi::ComputeState State;
        State.setPipeline(ComputePipeline);
        State.addBindingSet(Bindings);
        CmdList->setComputeState(State);
        CmdList->dispatch(dispatchX, dispatchY, 1);

        // =====================================================================
        // Frame dump
        // =====================================================================
        if (FrameDumper.IsEnabled()) {
            FrameDumper.BeginDump(NvrhiDevice, HDROutputTexture.Get(), CurrentFBInfo.width, CurrentFBInfo.height);
            FrameDumper.PrepareCopy(CmdList);
        }

        // =====================================================================
        // Blit to screen
        // =====================================================================
        CmdList->setTextureState(HDROutputTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

        FCommonRenderPasses::BlitParameters BlitParams;
        FCommonRenderPasses::BlitTexture(
            CmdList,
            Framebuffer,
            HDROutputTexture,
            &BindingCache,
            CurrentFBInfo.width,
            CurrentFBInfo.height,
            BlitParams);

        CmdList->close();
        NvrhiDevice->executeCommandList(CmdList);
        NvrhiDevice->waitForIdle();

        // =====================================================================
        // Frame dump readback
        // =====================================================================
        if (FrameDumper.IsEnabled()) {
            if (FrameDumper.ReadbackAndSave()) {
                HLVM_LOG(LogTest, info, TXT("Dumped frame {}"), FrameDumper.GetCurrentFrame());
            }
            if (FrameDumper.IsLastFrame()) {
                return;
            }
        }
    }

    virtual void BackBufferResizing() override
    {
        HDROutputTexture = nullptr;
        BindingCache.Clear();
    }

private:
    nvrhi::IDevice*          NvrhiDevice = nullptr;
    nvrhi::FramebufferInfo   FBInfo;
    FString                  WindowTitle;

    nvrhi::ShaderHandle         LightingCS;
    nvrhi::BindingLayoutHandle  BindingLayout;
    nvrhi::ComputePipelineHandle ComputePipeline;
    nvrhi::BufferHandle         ConstantBuffer;
    nvrhi::TextureHandle        HDROutputTexture;

    FBindingCache       BindingCache;
    FRenderPassDumper   FrameDumper;

    uint32_t LastWidth = 0;
    uint32_t LastHeight = 0;
    uint32_t FrameCount = 0;
    float    FPSUpdateTimer = 0.0f;
};

// =============================================================================
// TEST IMPLEMENTATION
// =============================================================================

RECORD_BOOL(test_PBRLighting)
{
    HLVM_LOG(LogTest, info, TXT("=== Starting PBR Lighting Test ==="));

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
        DeviceParams.bEnableDebugRuntime = true;
        DeviceParams.bEnableNVRHIValidationLayer = true;
        DeviceParams.bEnableRayTracingExtensions = false;

        if (!DeviceManager->CreateWindowDeviceAndSwapChain(WindowProps))
        {
            throw std::runtime_error("Failed to create window, device and swap chain");
        }

        nvrhi::IDevice*      NvrhiDevice = DeviceManager->GetDevice();
        nvrhi::IFramebuffer* FirstFB = DeviceManager->GetFramebuffer(0);

        HLVM_LOG(LogTest, info, TXT("Creating render pass..."));
        TSharedPtr<FPBRLightingPass> PBRLightingPass =
            std::make_shared<FPBRLightingPass>(DeviceManager.get());
        if (!PBRLightingPass->Initialize(NvrhiDevice, FirstFB, FString(TXT("PBR Lighting"))))
        {
            throw std::runtime_error("Failed to initialize FPBRLightingPass");
        }

        DeviceManager->AddRenderPassToBack(PBRLightingPass);

        HLVM_LOG(LogTest, info, TXT("Starting render loop..."));

        std::thread([&]() {
            FTimer Timer;
            while (Timer.MarkSec() < 3.0)
            {
            }
            DeviceManager->StopMessageLoop();
        }).detach();

        DeviceManager->RunMessageLoop();

        PBRLightingPass->Shutdown();

        HLVM_LOG(LogTest, info, TXT("Test completed successfully!"));
        return true;
    }
    catch (const std::exception& e)
    {
        HLVM_LOG(LogTest, critical, TXT("Test failed: {}"), TO_TCHAR_CSTR(e.what()));
        return false;
    }
}

#else // HLVM_VULKAN_RENDERER

RECORD_BOOL(test_PBRLighting)
{
    HLVM_LOG(LogTest, warning, TXT("Vulkan renderer not enabled - skipping test"));
    return true;
}

#endif // HLVM_VULKAN_RENDERER
