/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * GPU Instancing Test
 *
 * Validates instanced rendering for both GBuffer and Shadow passes.
 * Renders 100 cubes in a 10x10 grid using a single draw call per pass.
 */

#include "Test.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Deferred/FGBufferFillPass.h"
#include "Renderer/Shadow/FShadowMapPass.h"
#include "Renderer/RHI/Object/Buffer.h"
#include "Renderer/Mesh/IMesh.h"
#include <nvrhi/utils.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

DECLARE_LOG_CATEGORY(LogTestGPUInstancing)

#if HLVM_VULKAN_RENDERER

// =============================================================================
// CONFIGURATION
// =============================================================================

static const char*    WINDOW_TITLE = "GPU Instancing Test";
static const uint32_t WINDOW_WIDTH = 512;
static const uint32_t WINDOW_HEIGHT = 512;
static const uint32_t INSTANCE_COUNT = 100;
static const uint32_t GRID_SIZE = 10;

// =============================================================================
// TEST CONTEXT
// =============================================================================

struct FGPUInstancingTestContext
{
    // Device
    TUniquePtr<FDeviceManager> DeviceManager;
    nvrhi::IDevice*            NvrhiDevice = nullptr;
    nvrhi::CommandListHandle   CmdList;

    // Passes
    FGBufferFillPass GBufferPass;
    FShadowMapPass   ShadowPass;

    // Geometry
    TUniquePtr<FStaticVertexBuffer> CubeVertexBuffer;
    TUniquePtr<FStaticIndexBuffer>  CubeIndexBuffer;
    uint32_t                        CubeIndexCount = 0;

    // Instance data
    nvrhi::BufferHandle InstanceBuffer;

    // Material placeholder textures
    nvrhi::TextureHandle WhiteTexture;
    nvrhi::TextureHandle BlueNormalTexture;
};

// =============================================================================
// CUBE GEOMETRY (FVertex format: Position, Normal, UV, Tangent)
// =============================================================================

static const FVertex CubeVertices[24] = {
    // Front face (z = +0.5)
    { FVec3(-0.5f, -0.5f,  0.5f), FVec3(0, 0, 1), FVec2(0, 0), FVec3(1, 0, 0) },
    { FVec3( 0.5f, -0.5f,  0.5f), FVec3(0, 0, 1), FVec2(1, 0), FVec3(1, 0, 0) },
    { FVec3( 0.5f,  0.5f,  0.5f), FVec3(0, 0, 1), FVec2(1, 1), FVec3(1, 0, 0) },
    { FVec3(-0.5f,  0.5f,  0.5f), FVec3(0, 0, 1), FVec2(0, 1), FVec3(1, 0, 0) },
    // Back face (z = -0.5)
    { FVec3( 0.5f, -0.5f, -0.5f), FVec3(0, 0, -1), FVec2(0, 0), FVec3(-1, 0, 0) },
    { FVec3(-0.5f, -0.5f, -0.5f), FVec3(0, 0, -1), FVec2(1, 0), FVec3(-1, 0, 0) },
    { FVec3(-0.5f,  0.5f, -0.5f), FVec3(0, 0, -1), FVec2(1, 1), FVec3(-1, 0, 0) },
    { FVec3( 0.5f,  0.5f, -0.5f), FVec3(0, 0, -1), FVec2(0, 1), FVec3(-1, 0, 0) },
    // Top face (y = +0.5)
    { FVec3(-0.5f,  0.5f, -0.5f), FVec3(0, 1, 0), FVec2(0, 0), FVec3(1, 0, 0) },
    { FVec3( 0.5f,  0.5f, -0.5f), FVec3(0, 1, 0), FVec2(1, 0), FVec3(1, 0, 0) },
    { FVec3( 0.5f,  0.5f,  0.5f), FVec3(0, 1, 0), FVec2(1, 1), FVec3(1, 0, 0) },
    { FVec3(-0.5f,  0.5f,  0.5f), FVec3(0, 1, 0), FVec2(0, 1), FVec3(1, 0, 0) },
    // Bottom face (y = -0.5)
    { FVec3(-0.5f, -0.5f,  0.5f), FVec3(0, -1, 0), FVec2(0, 0), FVec3(1, 0, 0) },
    { FVec3( 0.5f, -0.5f,  0.5f), FVec3(0, -1, 0), FVec2(1, 0), FVec3(1, 0, 0) },
    { FVec3( 0.5f, -0.5f, -0.5f), FVec3(0, -1, 0), FVec2(1, 1), FVec3(1, 0, 0) },
    { FVec3(-0.5f, -0.5f, -0.5f), FVec3(0, -1, 0), FVec2(0, 1), FVec3(1, 0, 0) },
    // Right face (x = +0.5)
    { FVec3( 0.5f, -0.5f,  0.5f), FVec3(1, 0, 0), FVec2(0, 0), FVec3(0, 0, -1) },
    { FVec3( 0.5f,  0.5f,  0.5f), FVec3(1, 0, 0), FVec2(1, 0), FVec3(0, 0, -1) },
    { FVec3( 0.5f,  0.5f, -0.5f), FVec3(1, 0, 0), FVec2(1, 1), FVec3(0, 0, -1) },
    { FVec3( 0.5f, -0.5f, -0.5f), FVec3(1, 0, 0), FVec2(0, 1), FVec3(0, 0, -1) },
    // Left face (x = -0.5)
    { FVec3(-0.5f, -0.5f, -0.5f), FVec3(-1, 0, 0), FVec2(0, 0), FVec3(0, 0, 1) },
    { FVec3(-0.5f,  0.5f, -0.5f), FVec3(-1, 0, 0), FVec2(1, 0), FVec3(0, 0, 1) },
    { FVec3(-0.5f,  0.5f,  0.5f), FVec3(-1, 0, 0), FVec2(1, 1), FVec3(0, 0, 1) },
    { FVec3(-0.5f, -0.5f,  0.5f), FVec3(-1, 0, 0), FVec2(0, 1), FVec3(0, 0, 1) },
};

static const uint32_t CubeIndices[36] = {
    0,  1,  2,   2,  3,  0,   // Front
    4,  5,  6,   6,  7,  4,   // Back
    8,  9,  10,  10, 11, 8,   // Top
    12, 13, 14,  14, 15, 12,  // Bottom
    16, 17, 18,  18, 19, 16,  // Right
    20, 21, 22,  22, 23, 20,  // Left
};

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static nvrhi::TextureHandle CreateSolidTexture(nvrhi::IDevice* Device, nvrhi::CommandListHandle CmdList,
    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    nvrhi::TextureDesc Desc;
    Desc.setWidth(1).setHeight(1).setFormat(nvrhi::Format::RGBA8_UNORM)
        .setIsUAV(false).setInitialState(nvrhi::ResourceStates::ShaderResource)
        .setKeepInitialState(true);
    nvrhi::TextureHandle Tex = Device->createTexture(Desc);

    uint8_t pixels[4] = { r, g, b, a };
    CmdList->writeTexture(Tex, 0, 0, pixels, sizeof(pixels));

    return Tex;
}

static bool InitializeTest(FGPUInstancingTestContext& Context)
{
    HLVM_LOG(LogTestGPUInstancing, info, TXT("Initializing GPU Instancing Test..."));

    // 1. Create window and device
    IWindow::Properties WindowProps;
    WindowProps.Title = WINDOW_TITLE;
    WindowProps.Extent = { WINDOW_WIDTH, WINDOW_HEIGHT };
    WindowProps.Resizable = false;
    WindowProps.VSync = IWindow::EVsync::Off;

    Context.DeviceManager = FDeviceManager::Create(nvrhi::GraphicsAPI::VULKAN);
    if (!Context.DeviceManager)
    {
        HLVM_LOG(LogTestGPUInstancing, err, TXT("Failed to create DeviceManager"));
        return false;
    }

    FDeviceCreationParameters& DeviceParams = const_cast<FDeviceCreationParameters&>(Context.DeviceManager->GetDeviceParams());
    DeviceParams.BackBufferWidth = WINDOW_WIDTH;
    DeviceParams.BackBufferHeight = WINDOW_HEIGHT;
    DeviceParams.SwapChainBufferCount = 2;
    DeviceParams.VSyncMode = 0;
    DeviceParams.bEnableDebugRuntime = HLVM_BUILD_DEBUG;
    DeviceParams.bEnableNVRHIValidationLayer = HLVM_BUILD_DEBUG;
    DeviceParams.bEnableRayTracingExtensions = false;

    if (!Context.DeviceManager->CreateWindowDeviceAndSwapChain(WindowProps))
    {
        HLVM_LOG(LogTestGPUInstancing, err, TXT("Failed to create device and swapchain"));
        return false;
    }

    Context.NvrhiDevice = Context.DeviceManager->GetDevice();
    if (!Context.NvrhiDevice)
    {
        HLVM_LOG(LogTestGPUInstancing, err, TXT("Failed to get NVRHI device"));
        return false;
    }

    // 2. Create command list
    nvrhi::CommandListParameters Params = {};
    Params.enableImmediateExecution = false;
    Context.CmdList = Context.NvrhiDevice->createCommandList(Params);
    Context.CmdList->open();

    // 3. Create cube geometry
    Context.CubeVertexBuffer = TUniquePtr<FStaticVertexBuffer>(new FStaticVertexBuffer());
    Context.CubeVertexBuffer->Initialize(Context.CmdList.Get(), Context.NvrhiDevice, CubeVertices, sizeof(CubeVertices));

    Context.CubeIndexBuffer = TUniquePtr<FStaticIndexBuffer>(new FStaticIndexBuffer());
    Context.CubeIndexBuffer->Initialize(Context.CmdList.Get(), Context.NvrhiDevice, CubeIndices, sizeof(CubeIndices), nvrhi::Format::R32_UINT);
    Context.CubeIndexCount = 36;

    // 4. Create placeholder textures
    Context.WhiteTexture = CreateSolidTexture(Context.NvrhiDevice, Context.CmdList, 255, 255, 255, 255);
    Context.BlueNormalTexture = CreateSolidTexture(Context.NvrhiDevice, Context.CmdList, 128, 128, 255, 255); // tangent-space normal pointing up

    // 5. Create instance matrices (10x10 grid)
    std::vector<glm::mat4> InstanceMatrices;
    InstanceMatrices.reserve(INSTANCE_COUNT);
    for (uint32_t x = 0; x < GRID_SIZE; ++x)
    {
        for (uint32_t z = 0; z < GRID_SIZE; ++z)
        {
            glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(
                (float(x) - float(GRID_SIZE) * 0.5f) * 2.0f,
                0.0f,
                (float(z) - float(GRID_SIZE) * 0.5f) * 2.0f));
            InstanceMatrices.push_back(m);
        }
    }

    // 6. Create instance buffer
    nvrhi::BufferDesc InstanceBufferDesc;
    InstanceBufferDesc.setByteSize(InstanceMatrices.size() * sizeof(glm::mat4))
        .setStructStride(sizeof(glm::mat4))
        .setInitialState(nvrhi::ResourceStates::ShaderResource)
        .setKeepInitialState(true);
    Context.InstanceBuffer = Context.NvrhiDevice->createBuffer(InstanceBufferDesc);
    Context.CmdList->writeBuffer(Context.InstanceBuffer, InstanceMatrices.data(), InstanceMatrices.size() * sizeof(glm::mat4));

    // 7. Initialize passes
    const auto ShaderDataDir = FString::Format(
        TXT("{}/Engine/Source/Runtime/Test/TestGPUInstancing_Data"),
        *GProjectRoot);

    FGBufferFillPass::FDesc GBufferDesc;
    GBufferDesc.Width = WINDOW_WIDTH;
    GBufferDesc.Height = WINDOW_HEIGHT;
    if (!Context.GBufferPass.Initialize(Context.NvrhiDevice, ShaderDataDir, GBufferDesc))
    {
        HLVM_LOG(LogTestGPUInstancing, err, TXT("Failed to initialize GBuffer pass"));
        return false;
    }

    FShadowMapPass::FDesc ShadowDesc;
    ShadowDesc.ShadowMapSize = 512;
    if (!Context.ShadowPass.Initialize(Context.NvrhiDevice, ShaderDataDir, ShadowDesc))
    {
        HLVM_LOG(LogTestGPUInstancing, err, TXT("Failed to initialize shadow pass"));
        return false;
    }

    Context.CmdList->close();
    Context.NvrhiDevice->executeCommandList(Context.CmdList);
    Context.NvrhiDevice->waitForIdle();

    HLVM_LOG(LogTestGPUInstancing, info, TXT("Initialization complete"));
    return true;
}

static bool RunTest(FGPUInstancingTestContext& Context)
{
    HLVM_LOG(LogTestGPUInstancing, info, TXT("Running GPU Instancing Test..."));

    // Camera matrices
    glm::vec3 CameraPos(0.0f, 15.0f, 25.0f);
    glm::vec3 Target(0.0f, 0.0f, 0.0f);
    glm::mat4 View = glm::lookAtLH(CameraPos, Target, glm::vec3(0.0f, 1.0f, 0.0f));
    float aspect = float(WINDOW_WIDTH) / float(WINDOW_HEIGHT);
    glm::mat4 Proj = glm::perspectiveLH_ZO(glm::radians(60.0f), aspect, 0.1f, 1000.0f);

    // Light matrices
    glm::vec3 LightPos(10.0f, 20.0f, 10.0f);
    glm::mat4 LightView = glm::lookAtLH(LightPos, Target, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 LightProj = glm::orthoLH_ZO(-20.0f, 20.0f, -20.0f, 20.0f, 0.1f, 100.0f);
    glm::mat4 LightViewProj = LightProj * LightView;

    // Build GBuffer instanced draw item
    FGBufferFillPass::FMaterialBinding Material;
    Material.DiffuseTexture = Context.WhiteTexture;
    Material.NormalTexture = Context.BlueNormalTexture;
    Material.MetallicTexture = Context.WhiteTexture;
    Material.RoughnessTexture = Context.WhiteTexture;
    Material.AOTexture = Context.WhiteTexture;
    Material.Constants.AlbedoTint[0] = 0.8f;
    Material.Constants.AlbedoTint[1] = 0.3f;
    Material.Constants.AlbedoTint[2] = 0.3f;
    Material.Constants.AlbedoTint[3] = 1.0f;
    Material.Constants.Metallic = 0.0f;
    Material.Constants.Roughness = 0.5f;
    Material.Constants.EmissiveStrength = 0.0f;

    FGBufferFillPass::FInstancedMeshDrawItem GBufferItem;
    GBufferItem.VertexBuffer = Context.CubeVertexBuffer->GetBufferHandle().Get();
    GBufferItem.IndexBuffer = Context.CubeIndexBuffer->GetBufferHandle().Get();
    GBufferItem.InstanceBuffer = Context.InstanceBuffer;
    GBufferItem.IndexCount = Context.CubeIndexCount;
    GBufferItem.InstanceCount = INSTANCE_COUNT;
    GBufferItem.Material = Material;

    FGBufferFillPass::FViewConstants ViewConstants;
    memset(&ViewConstants, 0, sizeof(ViewConstants));
    memcpy(ViewConstants.ViewMatrix, glm::value_ptr(View), 64);
    memcpy(ViewConstants.ProjMatrix, glm::value_ptr(Proj), 64);
    ViewConstants.CameraPos[0] = CameraPos.x;
    ViewConstants.CameraPos[1] = CameraPos.y;
    ViewConstants.CameraPos[2] = CameraPos.z;
    ViewConstants.CameraPos[3] = 1.0f;

    FGBufferFillPass::FInstancedRenderDesc GBufferRenderDesc;
    GBufferRenderDesc.ViewConstants = ViewConstants;
    GBufferRenderDesc.InstancedItems = &GBufferItem;
    GBufferRenderDesc.InstancedItemCount = 1;

    // Build shadow instanced draw item
    FShadowMapPass::FInstancedMeshDrawItem ShadowItem;
    ShadowItem.VertexBuffer = Context.CubeVertexBuffer->GetBufferHandle().Get();
    ShadowItem.IndexBuffer = Context.CubeIndexBuffer->GetBufferHandle().Get();
    ShadowItem.InstanceBuffer = Context.InstanceBuffer;
    ShadowItem.IndexCount = Context.CubeIndexCount;
    ShadowItem.InstanceCount = INSTANCE_COUNT;

    FShadowMapPass::FInstancedRenderDesc ShadowRenderDesc;
    ShadowRenderDesc.LightViewProj = LightViewProj;
    ShadowRenderDesc.InstancedItems = &ShadowItem;
    ShadowRenderDesc.InstancedItemCount = 1;

    // Render
    Context.CmdList = Context.NvrhiDevice->createCommandList();
    Context.CmdList->open();

    // GBuffer instanced render
    Context.GBufferPass.RenderInstanced(Context.CmdList.Get(), GBufferRenderDesc);

    // Shadow instanced render
    Context.ShadowPass.RenderInstanced(Context.CmdList.Get(), ShadowRenderDesc);

    Context.CmdList->close();
    Context.NvrhiDevice->executeCommandList(Context.CmdList);
    Context.NvrhiDevice->waitForIdle();

    HLVM_LOG(LogTestGPUInstancing, info, TXT("Rendering complete. Validating..."));

    // Read back GBuffer diffuse texture and check for non-black pixels
    nvrhi::TextureHandle DiffuseTex = Context.GBufferPass.GetDiffuseTexture();
    if (!DiffuseTex)
    {
        HLVM_LOG(LogTestGPUInstancing, err, TXT("Diffuse texture is null"));
        return false;
    }

    // Create staging texture for readback
    nvrhi::TextureDesc StagingDesc = DiffuseTex->getDesc();
    StagingDesc.isRenderTarget = false;
    StagingDesc.isUAV = false;
    StagingDesc.initialState = nvrhi::ResourceStates::CopyDest;
    StagingDesc.keepInitialState = false;
    nvrhi::StagingTextureHandle StagingTex = Context.NvrhiDevice->createStagingTexture(
        StagingDesc, nvrhi::CpuAccessMode::Read);
    if (!StagingTex)
    {
        HLVM_LOG(LogTestGPUInstancing, err, TXT("Failed to create staging texture"));
        return false;
    }

    // Copy texture to staging
    Context.CmdList = Context.NvrhiDevice->createCommandList();
    Context.CmdList->open();
    Context.CmdList->copyTexture(StagingTex, nvrhi::TextureSlice(), DiffuseTex, nvrhi::TextureSlice());
    Context.CmdList->close();
    Context.NvrhiDevice->executeCommandList(Context.CmdList);
    Context.NvrhiDevice->waitForIdle();

    size_t rowPitch = 0;
    void* mappedData = Context.NvrhiDevice->mapStagingTexture(
        StagingTex.Get(), nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, &rowPitch);
    if (!mappedData)
    {
        HLVM_LOG(LogTestGPUInstancing, err, TXT("Failed to map staging texture"));
        return false;
    }

    bool foundNonBlack = false;
    uint8_t* pixels = static_cast<uint8_t*>(mappedData);
    for (uint32_t y = 0; y < WINDOW_HEIGHT && !foundNonBlack; ++y)
    {
        for (uint32_t x = 0; x < WINDOW_WIDTH && !foundNonBlack; ++x)
        {
            uint8_t* pixel = pixels + y * rowPitch + x * 4;
            if (pixel[0] > 10 || pixel[1] > 10 || pixel[2] > 10)
            {
                foundNonBlack = true;
            }
        }
    }
    Context.NvrhiDevice->unmapStagingTexture(StagingTex.Get());

    if (!foundNonBlack)
    {
        HLVM_LOG(LogTestGPUInstancing, err, TXT("GBuffer diffuse is all black — instancing failed"));
        return false;
    }

    HLVM_LOG(LogTestGPUInstancing, info, TXT("GBuffer diffuse validation passed (non-black pixels found)"));

    // Read back shadow map and check for valid depth
    nvrhi::TextureHandle ShadowTex = Context.ShadowPass.GetShadowMapTexture();
    if (!ShadowTex)
    {
        HLVM_LOG(LogTestGPUInstancing, err, TXT("Shadow map texture is null"));
        return false;
    }

    nvrhi::TextureDesc ShadowStagingDesc = ShadowTex->getDesc();
    ShadowStagingDesc.isRenderTarget = false;
    ShadowStagingDesc.isUAV = false;
    ShadowStagingDesc.initialState = nvrhi::ResourceStates::CopyDest;
    ShadowStagingDesc.keepInitialState = false;
    nvrhi::StagingTextureHandle ShadowStaging = Context.NvrhiDevice->createStagingTexture(
        ShadowStagingDesc, nvrhi::CpuAccessMode::Read);

    Context.CmdList = Context.NvrhiDevice->createCommandList();
    Context.CmdList->open();
    Context.CmdList->copyTexture(ShadowStaging, nvrhi::TextureSlice(), ShadowTex, nvrhi::TextureSlice());
    Context.CmdList->close();
    Context.NvrhiDevice->executeCommandList(Context.CmdList);
    Context.NvrhiDevice->waitForIdle();

    size_t shadowRowPitch = 0;
    bool foundValidDepth = false;
    void* shadowMapped = Context.NvrhiDevice->mapStagingTexture(
        ShadowStaging.Get(), nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, &shadowRowPitch);
    if (shadowMapped)
    {
        float* depthPixels = static_cast<float*>(shadowMapped);
        for (uint32_t y = 0; y < 512 && !foundValidDepth; ++y)
        {
            for (uint32_t x = 0; x < 512 && !foundValidDepth; ++x)
            {
                float depth = depthPixels[y * (shadowRowPitch / sizeof(float)) + x];
                if (depth < 0.99f)
                {
                    foundValidDepth = true;
                }
            }
        }
        Context.NvrhiDevice->unmapStagingTexture(ShadowStaging.Get());
    }

    if (!foundValidDepth)
    {
        HLVM_LOG(LogTestGPUInstancing, err, TXT("Shadow map is all white (depth = 1.0) — instancing failed"));
        return false;
    }

    HLVM_LOG(LogTestGPUInstancing, info, TXT("Shadow map validation passed (valid depth found)"));
    return true;
}

static void ShutdownTest(FGPUInstancingTestContext& Context)
{
    HLVM_LOG(LogTestGPUInstancing, info, TXT("Shutting down..."));

    Context.GBufferPass.Shutdown();
    Context.ShadowPass.Shutdown();

    Context.InstanceBuffer = nullptr;
    Context.WhiteTexture = nullptr;
    Context.BlueNormalTexture = nullptr;

    Context.CubeIndexBuffer = nullptr;
    Context.CubeVertexBuffer = nullptr;

    Context.CmdList.Reset();
    Context.DeviceManager = nullptr;
    Context.NvrhiDevice = nullptr;
}

// =============================================================================
// TEST ENTRY POINT
// =============================================================================

RECORD_BOOL(TestGPUInstancing, true)
{
    FGPUInstancingTestContext Context;

    try
    {
        HLVM_ENSURE(InitializeTest(Context));
        bool Result = RunTest(Context);
        ShutdownTest(Context);
        return Result;
    }
    catch (const std::exception& e)
    {
        ShutdownTest(Context);
        HLVM_LOG(LogTestGPUInstancing, critical, TXT("Test exception: {}"), TO_TCHAR_CSTR(e.what()));
        return false;
    }
    catch (...)
    {
        ShutdownTest(Context);
        HLVM_LOG(LogTestGPUInstancing, critical, TXT("Unknown exception"));
        return false;
    }
}

#endif // HLVM_VULKAN_RENDERER
