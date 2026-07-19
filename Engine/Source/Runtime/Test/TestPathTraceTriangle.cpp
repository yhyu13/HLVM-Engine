/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * TestPathTraceTriangle - Minimal path-tracing sanity check.
 *
 * Renders a single triangle with a tiny two-bounce RT shader using the same
 * NVRHI ray-tracing pipeline that FGIPass uses.  This is smaller than the
 * Cornell Box test and is intended to unblock path-tracing debugging.
 */

#include "Test.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Common/FBindingCache.h"
#include "Renderer/Common/FCommonRenderPasses.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
#include "Platform/FileSystem/Path.h"
#include <nvrhi/utils.h>
#include <Utility/Timer.h>
#include <fstream>

DECLARE_LOG_CATEGORY(LogTest)

#if HLVM_VULKAN_RENDERER

// =============================================================================
// CONFIGURATION
// =============================================================================

static const char*    WINDOW_TITLE = "Path Trace Triangle MVP";
static const uint32_t WIDTH = 400;
static const uint32_t HEIGHT = 300;

// =============================================================================
// HELPERS
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
// FPathTraceTrianglePass
// =============================================================================

class FPathTraceTrianglePass : public IRenderPass
{
public:
    using IRenderPass::IRenderPass;

    bool Initialize(nvrhi::IDevice* Device, nvrhi::IFramebuffer* Framebuffer, const FString& InWindowTitle)
    {
        NvrhiDevice = Device;
        FBInfo = Framebuffer->getFramebufferInfo();
        WindowTitle = InWindowTitle;

        BindingCache.SetDevice(NvrhiDevice);

        const auto DataDir = FString::Format(
            TXT("{}/../../Test/{}_Data"), *GExecutablePath, *GExecutableName);

        // Load ray tracing shader library from ShaderMake blob.
        auto ShaderBlob = ReadBinaryFile(
            FPath::Combine(DataDir, TXT("PathTraceTriangle.sblob")).string());

        const void* ShaderBinary = nullptr;
        size_t      ShaderBinarySize = 0;
        if (!ShaderMake::FindPermutationInBlob(
                ShaderBlob.data(), ShaderBlob.size(), nullptr, 0,
                &ShaderBinary, &ShaderBinarySize))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to extract PathTraceTriangle shader from blob"));
            return false;
        }

        ShaderLibrary = NvrhiDevice->createShaderLibrary(ShaderBinary, ShaderBinarySize);
        if (!ShaderLibrary)
        {
            HLVM_LOG(LogTest, err, TXT("Failed to load path tracing shader library"));
            return false;
        }

        RayGenShader = ShaderLibrary->getShader("RayGen", nvrhi::ShaderType::RayGeneration);
        ClosestHitShader = ShaderLibrary->getShader("ClosestHit", nvrhi::ShaderType::ClosestHit);
        MissShader = ShaderLibrary->getShader("Miss", nvrhi::ShaderType::Miss);

        if (!RayGenShader || !ClosestHitShader || !MissShader)
        {
            HLVM_LOG(LogTest, err, TXT("Failed to get path tracing shaders"));
            return false;
        }

        HLVM_LOG(LogTest, info, TXT("Path tracing shaders loaded successfully"));

        // Create command list for initialization.
        nvrhi::CommandListHandle InitCmdList = NvrhiDevice->createCommandList();
        InitCmdList->open();

        // Triangle geometry.
        nvrhi::BufferDesc BufferDesc;
        BufferDesc.byteSize = sizeof(uint32_t) * 3;
        BufferDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        BufferDesc.keepInitialState = true;
        BufferDesc.isAccelStructBuildInput = true;
        IndexBuffer = NvrhiDevice->createBuffer(BufferDesc);

        BufferDesc.byteSize = sizeof(float) * 9; // 3 vertices * 3 floats
        BufferDesc.isAccelStructBuildInput = true;
        VertexBuffer = NvrhiDevice->createBuffer(BufferDesc);

        uint32_t Indices[3] = { 0, 1, 2 };
        float    Vertices[9] = {
            0.0f, -1.0f, 1.0f, // vertex 0
            -1.0f, 1.0f, 1.0f, // vertex 1
            1.0f, 1.0f, 1.0f   // vertex 2
        };

        InitCmdList->writeBuffer(IndexBuffer, Indices, sizeof(Indices));
        InitCmdList->writeBuffer(VertexBuffer, Vertices, sizeof(Vertices));

        // BLAS.
        nvrhi::rt::AccelStructDesc BlasDesc;
        BlasDesc.isTopLevel = false;
        nvrhi::rt::GeometryDesc GeometryDesc;
        auto& Triangles = GeometryDesc.geometryData.triangles;
        Triangles.indexBuffer = IndexBuffer;
        Triangles.vertexBuffer = VertexBuffer;
        Triangles.indexFormat = nvrhi::Format::R32_UINT;
        Triangles.indexCount = 3;
        Triangles.vertexFormat = nvrhi::Format::RGB32_FLOAT;
        Triangles.vertexStride = sizeof(float) * 3;
        Triangles.vertexCount = 3;
        GeometryDesc.geometryType = nvrhi::rt::GeometryType::Triangles;
        GeometryDesc.flags = nvrhi::rt::GeometryFlags::Opaque;
        BlasDesc.bottomLevelGeometries.push_back(GeometryDesc);

        BottomLevelAS = NvrhiDevice->createAccelStruct(BlasDesc);
        nvrhi::utils::BuildBottomLevelAccelStruct(InitCmdList, BottomLevelAS, BlasDesc);

        // TLAS.
        nvrhi::rt::AccelStructDesc TlasDesc;
        TlasDesc.isTopLevel = true;
        TlasDesc.topLevelMaxInstances = 1;
        TopLevelAS = NvrhiDevice->createAccelStruct(TlasDesc);

        float Transform[12] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f
        };

        nvrhi::rt::InstanceDesc InstanceDesc;
        InstanceDesc.bottomLevelAS = BottomLevelAS;
        InstanceDesc.instanceMask = 1;
        InstanceDesc.flags = nvrhi::rt::InstanceFlags::TriangleFrontCounterclockwise;
        memcpy(InstanceDesc.transform, Transform, sizeof(Transform));

        InitCmdList->buildTopLevelAccelStruct(TopLevelAS, &InstanceDesc, 1);

        InitCmdList->close();
        NvrhiDevice->executeCommandList(InitCmdList);

        HLVM_LOG(LogTest, info, TXT("Acceleration structures built successfully"));

        // Binding layout.
        nvrhi::BindingLayoutDesc RayTracingLayoutDesc;
        RayTracingLayoutDesc.visibility = nvrhi::ShaderType::All;
        // Match ShaderMake's Vulkan register shifts: t=0, s=128, b=256, u=384.
        RayTracingLayoutDesc.bindingOffsets
            .setShaderResourceOffset(0)
            .setSamplerOffset(128)
            .setConstantBufferOffset(256)
            .setUnorderedAccessViewOffset(384);
        RayTracingLayoutDesc.bindings = {
            nvrhi::BindingLayoutItem::RayTracingAccelStruct(0),
            nvrhi::BindingLayoutItem::Texture_UAV(1)
        };
        BindingLayout = NvrhiDevice->createBindingLayout(RayTracingLayoutDesc);

        // Ray tracing pipeline.
        nvrhi::rt::PipelineDesc PipelineDesc;
        PipelineDesc.globalBindingLayouts = { BindingLayout };
        PipelineDesc.shaders = {
            { "", RayGenShader, nullptr },
            { "", MissShader, nullptr }
        };
        PipelineDesc.hitGroups = { { "HitGroup", ClosestHitShader, nullptr, nullptr, nullptr, false } };
        PipelineDesc.maxPayloadSize = sizeof(float) * 4;

        Pipeline = NvrhiDevice->createRayTracingPipeline(PipelineDesc);
        if (!Pipeline)
        {
            HLVM_LOG(LogTest, err, TXT("Failed to create ray tracing pipeline"));
            return false;
        }

        HLVM_LOG(LogTest, info, TXT("Ray tracing pipeline created successfully"));

        // Shader table.
        ShaderTable = Pipeline->createShaderTable();
        ShaderTable->setRayGenerationShader("RayGen");
        ShaderTable->addHitGroup("HitGroup");
        ShaderTable->addMissShader("Miss");

        HLVM_LOG(LogTest, info, TXT("Shader table created successfully"));

        CommandList = NvrhiDevice->createCommandList();

        HLVM_LOG(LogTest, info, TXT("FPathTraceTrianglePass initialized successfully"));
        return true;
    }

    void Shutdown()
    {
        BindingCache.Clear();

        CommandList = nullptr;
        BindingLayout = nullptr;
        ShaderTable = nullptr;
        Pipeline = nullptr;
        TopLevelAS = nullptr;
        BottomLevelAS = nullptr;
        VertexBuffer = nullptr;
        IndexBuffer = nullptr;
        RenderTarget = nullptr;
        RayTracingBindingSet = nullptr;
        ShaderLibrary = nullptr;
        RayGenShader = nullptr;
        ClosestHitShader = nullptr;
        MissShader = nullptr;

        HLVM_LOG(LogTest, info, TXT("FPathTraceTrianglePass shutdown complete"));
    }

    virtual void Animate(float fElapsedTimeSeconds) override
    {
        FrameCount++;
        FPSUpdateTimer += fElapsedTimeSeconds;
        if (FPSUpdateTimer >= 1.0f)
        {
            float FPS = float(FrameCount) / FPSUpdateTimer;
            WindowTitle = FString::Format(TXT("Path Trace Triangle MVP - FPS: {:.1f}"), FPS);
            if (auto* DM = GetDeviceManager())
            {
                DM->SetWindowTitle(WindowTitle);
            }
            FrameCount = 0;
            FPSUpdateTimer = 0.0f;
        }
    }

    virtual void Render(nvrhi::IFramebuffer* Framebuffer) override
    {
        if (!NvrhiDevice || !Framebuffer || !Pipeline)
            return;

        const auto& CurrentFBInfo = Framebuffer->getFramebufferInfo();

        if (!RenderTarget || CurrentFBInfo.width != LastWidth || CurrentFBInfo.height != LastHeight)
        {
            LastWidth = CurrentFBInfo.width;
            LastHeight = CurrentFBInfo.height;

            nvrhi::TextureDesc TextureDesc;
            TextureDesc.dimension = nvrhi::TextureDimension::Texture2D;
            TextureDesc.width = CurrentFBInfo.width;
            TextureDesc.height = CurrentFBInfo.height;
            TextureDesc.format = nvrhi::Format::RGBA32_FLOAT;
            TextureDesc.isUAV = true;
            TextureDesc.isRenderTarget = false;
            TextureDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
            TextureDesc.keepInitialState = true;
            TextureDesc.debugName = "PathTraceTriangleOutput";

            RenderTarget = NvrhiDevice->createTexture(TextureDesc);

            BindingCache.Clear();

            nvrhi::BindingSetDesc BindingSetDesc;
            BindingSetDesc.bindings = {
                nvrhi::BindingSetItem::RayTracingAccelStruct(0, TopLevelAS),
                nvrhi::BindingSetItem::Texture_UAV(1, RenderTarget)
            };
            RayTracingBindingSet = BindingCache.GetOrCreateBindingSet(BindingSetDesc, BindingLayout);
        }

        CommandList->open();

        // Path tracing pass.
        {
            nvrhi::rt::State RTState;
            RTState.shaderTable = ShaderTable;
            RTState.bindings = { RayTracingBindingSet };
            CommandList->setRayTracingState(RTState);

            nvrhi::rt::DispatchRaysArguments Args;
            Args.width = CurrentFBInfo.width;
            Args.height = CurrentFBInfo.height;
            CommandList->dispatchRays(Args);
        }

        // Blit to swapchain.
        FCommonRenderPasses::BlitParameters BlitParams;
        FCommonRenderPasses::BlitTexture(
            CommandList,
            Framebuffer,
            RenderTarget,
            &BindingCache,
            CurrentFBInfo.width,
            CurrentFBInfo.height,
            BlitParams);

        CommandList->close();
        NvrhiDevice->executeCommandList(CommandList);
    }

    virtual void BackBufferResizing() override
    {
        RenderTarget = nullptr;
        RayTracingBindingSet = nullptr;
        BindingCache.Clear();
    }

    bool ValidateOutput() const
    {
        if (!NvrhiDevice || !RenderTarget)
            return false;

        nvrhi::TextureDesc OutputDesc = RenderTarget->getDesc();

        nvrhi::TextureDesc StagingDesc;
        StagingDesc.dimension = nvrhi::TextureDimension::Texture2D;
        StagingDesc.width = OutputDesc.width;
        StagingDesc.height = OutputDesc.height;
        StagingDesc.format = OutputDesc.format;
        StagingDesc.isRenderTarget = false;
        StagingDesc.isUAV = false;
        StagingDesc.isTypeless = false;
        StagingDesc.initialState = nvrhi::ResourceStates::CopyDest;
        StagingDesc.keepInitialState = false;
        StagingDesc.debugName = "PathTraceTriangleStaging";
        nvrhi::StagingTextureHandle StagingTexture =
            NvrhiDevice->createStagingTexture(StagingDesc, nvrhi::CpuAccessMode::Read);

        nvrhi::CommandListHandle Cmd = NvrhiDevice->createCommandList();
        Cmd->open();
        Cmd->setTextureState(RenderTarget, nvrhi::AllSubresources, nvrhi::ResourceStates::CopySource);

        nvrhi::TextureSlice Slice = {};
        Slice.width = OutputDesc.width;
        Slice.height = OutputDesc.height;
        Slice.depth = 1;
        Cmd->copyTexture(StagingTexture, Slice, RenderTarget, Slice);

        Cmd->close();
        NvrhiDevice->executeCommandList(Cmd);
        NvrhiDevice->waitForIdle();

        size_t RowPitch = 0;
        void* MappedData = NvrhiDevice->mapStagingTexture(
            StagingTexture, Slice, nvrhi::CpuAccessMode::Read, &RowPitch);
        if (!MappedData)
            return false;

        // Sample a small window around the image center.
        const uint32_t CX = OutputDesc.width / 2;
        const uint32_t CY = OutputDesc.height / 2;
        const uint32_t Radius = 8;

        float SumR = 0.0f, SumG = 0.0f, SumB = 0.0f;
        uint32_t Samples = 0;

        for (uint32_t y = CY - Radius; y <= CY + Radius; ++y)
        {
            if (y >= OutputDesc.height)
                continue;
            const uint8_t* SrcRow = reinterpret_cast<const uint8_t*>(MappedData) +
                                    static_cast<size_t>(y) * RowPitch;
            const float* Src = reinterpret_cast<const float*>(SrcRow);
            for (uint32_t x = CX - Radius; x <= CX + Radius; ++x)
            {
                if (x >= OutputDesc.width)
                    continue;
                size_t Idx = x * 4;
                SumR += Src[Idx + 0];
                SumG += Src[Idx + 1];
                SumB += Src[Idx + 2];
                ++Samples;
            }
        }

        NvrhiDevice->unmapStagingTexture(StagingTexture);

        if (Samples == 0)
            return false;

        const float Inv = 1.0f / static_cast<float>(Samples);
        const float AvgR = SumR * Inv;
        const float AvgG = SumG * Inv;
        const float AvgB = SumB * Inv;

        HLVM_LOG(LogTest, info,
            TXT("ValidateOutput center average: R={:.3f} G={:.3f} B={:.3f}"),
            AvgR, AvgG, AvgB);

        // The background is ~0.05/0.07/0.1; the triangle center should be significantly brighter.
        return (AvgR + AvgG + AvgB) > 0.25f;
    }

    uint32_t GetFrameCount() const
    {
        return FrameCount;
    }

private:
    nvrhi::IDevice*          NvrhiDevice = nullptr;
    nvrhi::FramebufferInfo   FBInfo;
    FString                  WindowTitle;

    nvrhi::ShaderLibraryHandle ShaderLibrary;
    nvrhi::ShaderHandle        RayGenShader;
    nvrhi::ShaderHandle        ClosestHitShader;
    nvrhi::ShaderHandle        MissShader;

    nvrhi::rt::AccelStructHandle BottomLevelAS;
    nvrhi::rt::AccelStructHandle TopLevelAS;
    nvrhi::BufferHandle          IndexBuffer;
    nvrhi::BufferHandle          VertexBuffer;

    nvrhi::rt::PipelineHandle    Pipeline;
    nvrhi::rt::ShaderTableHandle ShaderTable;

    nvrhi::BindingLayoutHandle BindingLayout;
    nvrhi::TextureHandle       RenderTarget;
    nvrhi::BindingSetHandle    RayTracingBindingSet;

    FBindingCache BindingCache;

    nvrhi::CommandListHandle CommandList;

    uint32_t LastWidth = 0;
    uint32_t LastHeight = 0;

    uint32_t FrameCount = 0;
    float    FPSUpdateTimer = 0.0f;
};

// =============================================================================
// TEST IMPLEMENTATION
// =============================================================================

RECORD_BOOL(test_PathTraceTriangle)
{
    HLVM_LOG(LogTest, info, TXT("Starting Path Trace Triangle MVP Test..."));

    try
    {
        IWindow::Properties WindowProps;
        WindowProps.Title = WINDOW_TITLE;
        WindowProps.Extent = { WIDTH, HEIGHT };
        WindowProps.Resizable = true;
        WindowProps.VSync = IWindow::EVsync::Off;

        TUniquePtr<FDeviceManager> DeviceManager = FDeviceManager::Create(nvrhi::GraphicsAPI::VULKAN);
        if (!DeviceManager)
        {
            throw std::runtime_error("Failed to create DeviceManager");
        }

        FDeviceCreationParameters& DeviceParams = const_cast<FDeviceCreationParameters&>(
            DeviceManager->GetDeviceParams());
        DeviceParams.BackBufferWidth = WIDTH;
        DeviceParams.BackBufferHeight = HEIGHT;
        DeviceParams.SwapChainBufferCount = 2;
        DeviceParams.VSyncMode = 0;
        DeviceParams.bEnableDebugRuntime = true;
        DeviceParams.bEnableNVRHIValidationLayer = true;
        DeviceParams.bEnableRayTracingExtensions = true;

        if (!DeviceManager->CreateWindowDeviceAndSwapChain(WindowProps))
        {
            throw std::runtime_error("Failed to create window, device and swap chain");
        }

        nvrhi::IDevice* NvrhiDevice = DeviceManager->GetDevice();
        if (!NvrhiDevice->queryFeatureSupport(nvrhi::Feature::RayTracingPipeline))
        {
            throw std::runtime_error("Ray tracing not supported on this device");
        }

        nvrhi::IFramebuffer* FirstFB = DeviceManager->GetFramebuffer(0);

        TSharedPtr<FPathTraceTrianglePass> RTPass =
            std::make_shared<FPathTraceTrianglePass>(DeviceManager.get());
        if (!RTPass->Initialize(NvrhiDevice, FirstFB, FString(TXT("Path Trace Triangle MVP"))))
        {
            throw std::runtime_error("Failed to initialize FPathTraceTrianglePass");
        }

        DeviceManager->AddRenderPassToBack(RTPass);

        // Run just long enough to render a few frames.
        std::thread([&]() {
            FTimer Timer;
            while (Timer.MarkSec() < 0.5)
            {
            }
            DeviceManager->StopMessageLoop();
        }).detach();

        DeviceManager->RunMessageLoop();

        bool bValidation = RTPass->ValidateOutput();
        RTPass->Shutdown();

        if (!bValidation)
        {
            throw std::runtime_error("Output validation failed: triangle was not rendered");
        }

        HLVM_LOG(LogTest, info, TXT("Path Trace Triangle MVP completed successfully!"));
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

RECORD_BOOL(test_PathTraceTriangle)
{
    HLVM_LOG(LogTest, warning, TXT("Vulkan renderer not enabled - skipping test"));
    return true;
}

#endif // HLVM_VULKAN_RENDERER
