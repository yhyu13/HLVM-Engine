/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * TestToneMapping - ACES Filmic Tone Mapping Validation
 *
 * Creates a procedural HDR gradient (left=-5 EV, right=+5 EV).
 * Applies tone mapping + gamma correction via compute shader.
 * Cycles through modes: ACES -> Reinhard -> None for comparison.
 */

#include "Test.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Common/FBindingCache.h"
#include "Renderer/Common/FCommonRenderPasses.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
#include "Image/FRenderPassDumper.h"
#include <nvrhi/utils.h>
#include <glm/glm.hpp>

DECLARE_LOG_CATEGORY(LogTest)

#if HLVM_VULKAN_RENDERER

// =============================================================================
// CONFIGURATION
// =============================================================================

static const char*    WINDOW_TITLE = "Tone Mapping Test";
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
// FToneMappingPass
// =============================================================================

class FToneMappingPass : public IRenderPass
{
public:
    using IRenderPass::IRenderPass;

    bool Initialize(nvrhi::IDevice* Device, nvrhi::IFramebuffer* Framebuffer, const FString& InWindowTitle)
    {
        HLVM_LOG(LogTest, info, TXT("=== FToneMappingPass::Initialize ==="));

        NvrhiDevice = Device;
        FBInfo = Framebuffer->getFramebufferInfo();
        WindowTitle = InWindowTitle;
        BindingCache.SetDevice(NvrhiDevice);

        const auto ShaderDataDir = FString::Format(
            TXT("{}/Engine/Source/Runtime/Test/TestToneMapping_Data"),
            *GProjectRoot);

        // =====================================================================
        // Load compute shader
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Loading tonemap shader..."));

        auto CSBlob = ReadBinaryFile(
            FPath::Combine(ShaderDataDir, TXT("Tonemap_cs.sblob")).string());
        const void* CSBinary = nullptr;
        size_t      CSBinarySize = 0;
        if (!ShaderMake::FindPermutationInBlob(CSBlob.data(), CSBlob.size(), nullptr, 0, &CSBinary, &CSBinarySize))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to extract Tonemap_cs from blob"));
            return false;
        }

        nvrhi::ShaderDesc CSDesc;
        CSDesc.setShaderType(nvrhi::ShaderType::Compute);
        TonemapCS = NvrhiDevice->createShader(CSDesc, CSBinary, CSBinarySize);

        if (!TonemapCS)
        {
            HLVM_LOG(LogTest, err, TXT("Failed to create tonemap compute shader"));
            return false;
        }
        HLVM_LOG(LogTest, info, TXT("Tonemap shader loaded successfully"));

        // =====================================================================
        // Create SDR output texture
        // =====================================================================
        {
            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = WINDOW_WIDTH;
            Desc.height = WINDOW_HEIGHT;
            Desc.format = nvrhi::Format::RGBA32_FLOAT;
            Desc.isUAV = true;
            Desc.isShaderResource = true;
            Desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
            Desc.keepInitialState = true;
            Desc.debugName = "ToneMapping_SDROutput";
            SDROutputTexture = NvrhiDevice->createTexture(Desc);
        }

        // =====================================================================
        // Create constant buffer
        // =====================================================================
        {
            nvrhi::BufferDesc CBDesc;
            CBDesc.byteSize = sizeof(float) * 8; // 2 float4 registers
            CBDesc.isConstantBuffer = true;
            CBDesc.isVolatile = false;
            CBDesc.keepInitialState = true;
            CBDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
            CBDesc.debugName = "TonemapConstants";
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
            PipelineDesc.setComputeShader(TonemapCS);
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
        FrameDumper.SetTestName(TXT("TestToneMapping"));

        HLVM_LOG(LogTest, info, TXT("FToneMappingPass initialized successfully"));
        return true;
    }

    void Shutdown()
    {
        HLVM_LOG(LogTest, info, TXT("FToneMappingPass::Shutdown"));

        BindingCache.Clear();

        ComputePipeline = nullptr;
        BindingLayout = nullptr;
        TonemapCS = nullptr;
        HDRInputTexture = nullptr;
        SDROutputTexture = nullptr;
        ConstantBuffer = nullptr;
    }

    virtual void Animate(float fElapsedTimeSeconds) override
    {
        FrameCount++;
        FPSUpdateTimer += fElapsedTimeSeconds;
        float FPS = float(FrameCount) / FPSUpdateTimer;
        if (FPSUpdateTimer >= 1.0f)
        {
            WindowTitle = FString::Format(TXT("Tone Mapping - FPS: {:.1f}"), FPS);

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
        if (!SDROutputTexture || CurrentFBInfo.width != LastWidth || CurrentFBInfo.height != LastHeight)
        {
            LastWidth = CurrentFBInfo.width;
            LastHeight = CurrentFBInfo.height;

            // Recreate SDR output
            {
                nvrhi::TextureDesc Desc;
                Desc.dimension = nvrhi::TextureDimension::Texture2D;
                Desc.width = CurrentFBInfo.width;
                Desc.height = CurrentFBInfo.height;
                Desc.format = nvrhi::Format::RGBA32_FLOAT;
                Desc.isUAV = true;
                Desc.isShaderResource = true;
                Desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
                Desc.keepInitialState = true;
                Desc.debugName = "ToneMapping_SDROutput";
                SDROutputTexture = NvrhiDevice->createTexture(Desc);
            }

            BindingCache.Clear();
        }

        // Cycle through tone map modes every 30 frames
        int Mode = (FrameCount / 30) % 3;

        // =====================================================================
        // Compute Pass
        // =====================================================================
        nvrhi::CommandListParameters CmdListParams;
        CmdListParams.enableImmediateExecution = false;
        nvrhi::CommandListHandle CmdList = NvrhiDevice->createCommandList(CmdListParams);
        CmdList->open();

        // Write tonemap constants
        float TonemapData[8] = {
            1.0f, 2.2f, float(Mode), 0.0f,                       // Register 0
            float(CurrentFBInfo.width), float(CurrentFBInfo.height), 0.0f, 0.0f  // Register 1
        };
        CmdList->writeBuffer(ConstantBuffer, TonemapData, sizeof(TonemapData));

        // Transition SDR to UAV
        CmdList->setTextureState(SDROutputTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

        // Create binding set
        nvrhi::BindingSetDesc SetDesc;
        SetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(256, ConstantBuffer));
        SetDesc.addItem(nvrhi::BindingSetItem::Texture_UAV(384, SDROutputTexture));
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
        // Blit to screen
        // =====================================================================
        CmdList->setTextureState(SDROutputTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

        FCommonRenderPasses::BlitParameters BlitParams;
        FCommonRenderPasses::BlitTexture(
            CmdList,
            Framebuffer,
            SDROutputTexture,
            &BindingCache,
            CurrentFBInfo.width,
            CurrentFBInfo.height,
            BlitParams);

        // =====================================================================
        // Frame dump
        // =====================================================================
        if (FrameDumper.IsEnabled()) {
            FrameDumper.BeginDump(NvrhiDevice, SDROutputTexture.Get(), CurrentFBInfo.width, CurrentFBInfo.height);
            FrameDumper.PrepareCopy(CmdList);
        }

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
        HDRInputTexture = nullptr;
        SDROutputTexture = nullptr;
        BindingCache.Clear();
    }

private:
    nvrhi::IDevice*          NvrhiDevice = nullptr;
    nvrhi::FramebufferInfo   FBInfo;
    FString                  WindowTitle;

    nvrhi::ShaderHandle         TonemapCS;
    nvrhi::BindingLayoutHandle  BindingLayout;
    nvrhi::ComputePipelineHandle ComputePipeline;
    nvrhi::BufferHandle         ConstantBuffer;
    nvrhi::TextureHandle        HDRInputTexture;
    nvrhi::TextureHandle        SDROutputTexture;

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

RECORD_BOOL(test_ToneMapping)
{
    HLVM_LOG(LogTest, info, TXT("=== Starting Tone Mapping Test ==="));

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
        TSharedPtr<FToneMappingPass> ToneMappingPass =
            std::make_shared<FToneMappingPass>(DeviceManager.get());
        if (!ToneMappingPass->Initialize(NvrhiDevice, FirstFB, FString(TXT("Tone Mapping"))))
        {
            throw std::runtime_error("Failed to initialize FToneMappingPass");
        }

        DeviceManager->AddRenderPassToBack(ToneMappingPass);

        HLVM_LOG(LogTest, info, TXT("Starting render loop..."));

        std::thread([&]() {
            FTimer Timer;
            while (Timer.MarkSec() < 3.0)
            {
            }
            DeviceManager->StopMessageLoop();
        }).detach();

        DeviceManager->RunMessageLoop();

        ToneMappingPass->Shutdown();

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

RECORD_BOOL(test_ToneMapping)
{
    HLVM_LOG(LogTest, warning, TXT("Vulkan renderer not enabled - skipping test"));
    return true;
}

#endif // HLVM_VULKAN_RENDERER
