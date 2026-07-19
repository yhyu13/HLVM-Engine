/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * TestPathTraceGI - Reference path-tracing GI test using FGIPass.
 *
 * Validates the reusable FGIPass with a minimal RT scene:
 *  1. Cornell box scene (FCornellBoxScene), CPU-filled primary GBuffer
 *  2. Minimal TLAS/BLAS with one instance per quad
 *  3. FGIPass::DispatchRays
 *  4. Temporal accumulation + tonemap, blit to swapchain
 *
 * Debug/verification aids:
 *  - HLVM_DUMP_PTGI=1        dump Output/Accum/Display/Backbuffer PNGs on the last frame
 *  - HLVM_PT_CPU_REF=1       also dump a traditional CPU reference render
 *                            (CPUReference_{Direct,Albedo,Normal}.png) that validates
 *                            scene geometry/materials/camera/light without any GPU RT
 *  - HLVM_PT_DEBUG_MODE=N    shader debug visualisation (0=final,1=albedo,2=normal,
 *                            3=direct,4=indirect,5=firstHitDist,13/14=SRV sanity)
 *  - HLVM_PT_ACCUM_FRAMES=N  accumulation target (default 16)
 *  - HLVM_PT_EXPOSURE=F      pre-tonemap exposure (default 1.0)
 */

#include "Test.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Common/FCommonRenderPasses.h"
#include "Renderer/Common/FBindingLayoutBuilder.h"
#include "Renderer/GI/FGIPass.h"
#include "Renderer/Scene3D/FCornellBoxScene.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
#include "Platform/FileSystem/Path.h"
#include "Image/FImageDump.h"
#include <nvrhi/utils.h>
#include <filesystem>
#include <string>
#include <algorithm>
#include <limits>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <Utility/Timer.h>
#include <vector>

DECLARE_LOG_CATEGORY(LogTest)

#if HLVM_VULKAN_RENDERER

// =============================================================================
// CONFIGURATION
// =============================================================================

static const char*    WINDOW_TITLE = "Path Trace GI Test";
static const uint32_t WIDTH = 800;
static const uint32_t HEIGHT = 600;

// Number of frames to accumulate for a clean, low-noise path-traced image.
// Override with HLVM_PT_ACCUM_FRAMES environment variable for longer demos.
static const uint32_t DEFAULT_ACCUM_TARGET_FRAMES = 16;

// =============================================================================
// GPU-SIDE DATA LAYOUTS (must match GIPathTracing.hlsl)
// =============================================================================

struct FRTVertex {
    float Position[3];
    float Padding0;
    float Normal[3];
    float Padding1;
    float UV[2];
    float Padding2[2];
};
static_assert(sizeof(FRTVertex) == 48, "FRTVertex must be 48 bytes (std430-friendly)");

struct FInstanceInfo {
    uint32_t VertexOffset;
    uint32_t IndexOffset;
    uint32_t VertexCount;
    uint32_t IndexCount;
    float    AlbedoColor[3];
    uint32_t AlbedoTextureIndex;
    uint32_t MaterialFlags;
    uint32_t Padding[3];
};
static_assert(sizeof(FInstanceInfo) == 48, "FInstanceInfo must be 48 bytes");

struct FViewConstants {
    float Model[4][4];
    float View[4][4];
    float Proj[4][4];
    float RenderTargetSize[2];
    float FrameIndex;
    float Pad;
};

// =============================================================================
// HELPERS
// =============================================================================

static nvrhi::TextureHandle CreateTexture2D(
    nvrhi::IDevice* Device,
    uint32_t W, uint32_t H,
    nvrhi::Format Format,
    nvrhi::ResourceStates InitialState,
    const char* DebugName)
{
    nvrhi::TextureDesc Desc;
    Desc.dimension = nvrhi::TextureDimension::Texture2D;
    Desc.width = W;
    Desc.height = H;
    Desc.format = Format;
    Desc.initialState = InitialState;
    Desc.keepInitialState = true;
    Desc.debugName = DebugName;

    if (InitialState == nvrhi::ResourceStates::UnorderedAccess)
    {
        Desc.isUAV = true;
    }
    else if (InitialState == nvrhi::ResourceStates::RenderTarget)
    {
        Desc.isRenderTarget = true;
    }

    return Device->createTexture(Desc);
}

static std::vector<char> ReadBinaryFile(const std::string& Filename)
{
    std::ifstream file(Filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        return {};
    }
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    file.close();
    return buffer;
}

struct FCPUTriangle
{
    glm::vec3 V0;
    glm::vec3 V1;
    glm::vec3 V2;
    glm::vec3 Normal;
    glm::vec3 Albedo;
};

// =============================================================================
// CAMERA (shared by GPU view constants, CPU GBuffer fill, CPU reference render)
// =============================================================================

struct FCameraRig
{
    glm::vec3 Position;
    glm::vec3 Target;
    glm::vec3 Up;
    float     FovYDegrees;
    float     NearZ;
    float     FarZ;
};

// Classic Cornell-box viewpoint: inside the box near the front wall, looking at
// the back wall. 90-degree vertical FOV so the colored side walls plus the
// floor/ceiling are all in frame (a 60-degree FOV from the box center sees only
// a single flat wall - the old "uniform gray image" failure mode).
static FCameraRig GetCameraRig()
{
    FCameraRig Rig;
    Rig.Position    = glm::vec3(0.0f, 0.0f, 0.9f);
    Rig.Target      = glm::vec3(0.0f, 0.0f, -1.0f);
    Rig.Up          = glm::vec3(0.0f, 1.0f, 0.0f);
    Rig.FovYDegrees = 90.0f;
    Rig.NearZ       = 0.05f;
    Rig.FarZ        = 100.0f;
    return Rig;
}

static glm::mat4 GetCameraView(const FCameraRig& Rig)
{
    return glm::lookAt(Rig.Position, Rig.Target, Rig.Up);
}

static glm::mat4 GetCameraProj(const FCameraRig& Rig, uint32_t W, uint32_t H)
{
    return glm::perspective(glm::radians(Rig.FovYDegrees), float(W) / float(H), Rig.NearZ, Rig.FarZ);
}

// World-space primary ray for pixel (X, Y). The camera looks down -Z in view
// space (glm::perspective convention), so the unprojected direction uses z=-1.
static glm::vec3 MakeCameraRay(
    uint32_t X, uint32_t Y, uint32_t W, uint32_t H,
    const glm::mat4& InvView, const glm::mat4& InvProj)
{
    const float NdcX = (2.0f * (float(X) + 0.5f) / float(W)) - 1.0f;
    const float NdcY = 1.0f - (2.0f * (float(Y) + 0.5f) / float(H));

    glm::vec4 RayClip(NdcX, NdcY, -1.0f, 1.0f);
    glm::vec4 RayEye = InvProj * RayClip;
    RayEye = glm::vec4(RayEye.x, RayEye.y, -1.0f, 0.0f);
    return glm::normalize(glm::vec3(InvView * RayEye));
}

// =============================================================================
// FPathTraceGIPass
// =============================================================================

class FPathTraceGIPass : public IRenderPass
{
public:
    using IRenderPass::IRenderPass;

    bool Initialize(nvrhi::IDevice* Device, nvrhi::IFramebuffer* Framebuffer, const FString& InWindowTitle)
    {
        NvrhiDevice = Device;
        BindingCache.SetDevice(NvrhiDevice);
        FBInfo = Framebuffer->getFramebufferInfo();
        WindowTitle = InWindowTitle;

        const auto DataDir = FString::Format(
            TXT("{}/../../Test/{}_Data"), *GExecutablePath, *GExecutableName);

        HLVM_LOG(LogTest, info, TXT("TestPathTraceGI data dir: {}"), *DataDir);

        // Linear sampler
        {
            nvrhi::SamplerDesc Desc;
            Desc.setAddressU(nvrhi::SamplerAddressMode::Clamp)
                .setAddressV(nvrhi::SamplerAddressMode::Clamp)
                .setMinFilter(true)
                .setMagFilter(true)
                .setMipFilter(false);
            LinearSampler = NvrhiDevice->createSampler(Desc);
        }

        // View constants
        {
            nvrhi::BufferDesc Desc;
            Desc.byteSize = sizeof(FViewConstants);
            Desc.isConstantBuffer = true;
            Desc.initialState = nvrhi::ResourceStates::ConstantBuffer;
            Desc.keepInitialState = true;
            Desc.debugName = "PathTraceViewConstants";
            ViewConstants = NvrhiDevice->createBuffer(Desc);
        }

        // Scene geometry + TLAS
        if (!CreateSceneGeometry())
        {
            HLVM_LOG(LogTest, err, TXT("Failed to create scene geometry"));
            return false;
        }

        // GBuffer textures
        if (!CreateGBufferTextures())
        {
            HLVM_LOG(LogTest, err, TXT("Failed to create GBuffer textures"));
            return false;
        }

        // Output texture (raw HDR radiance from the path tracer)
        OutputTexture = CreateTexture2D(
            NvrhiDevice, WIDTH, HEIGHT, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "PathTraceOutput");

        // Temporal accumulation texture (running sum of raw radiance samples)
        AccumTexture = CreateTexture2D(
            NvrhiDevice, WIDTH, HEIGHT, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "PathTraceAccum");

        // Display texture (tonemapped moving average, blitted to swapchain)
        DisplayTexture = CreateTexture2D(
            NvrhiDevice, WIDTH, HEIGHT, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "PathTraceDisplay");

        // Staging texture for readback
        {
            nvrhi::TextureDesc StagingDesc;
            StagingDesc.dimension = nvrhi::TextureDimension::Texture2D;
            StagingDesc.width = WIDTH;
            StagingDesc.height = HEIGHT;
            StagingDesc.format = nvrhi::Format::RGBA32_FLOAT;
            StagingDesc.isRenderTarget = false;
            StagingDesc.isUAV = false;
            StagingDesc.isTypeless = false;
            StagingDesc.initialState = nvrhi::ResourceStates::CopyDest;
            StagingDesc.keepInitialState = false;
            StagingDesc.debugName = "PathTraceOutputStaging";
            StagingTexture = NvrhiDevice->createStagingTexture(StagingDesc, nvrhi::CpuAccessMode::Read);
        }

        // GI pass (uses scene lights loaded from CornellBox_Lights.json)
        if (!GIPass.Initialize(NvrhiDevice, DataDir, Scene.get()))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to initialize FGIPass"));
            return false;
        }

        // Temporal accumulation + tonemap compute pipeline
        if (!CreateAccumulationPipeline(DataDir))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to create accumulation pipeline"));
            return false;
        }

        AccumTargetFrames = DEFAULT_ACCUM_TARGET_FRAMES;
        if (const char* EnvFrames = std::getenv("HLVM_PT_ACCUM_FRAMES"))
        {
            try
            {
                int Parsed = std::stoi(EnvFrames);
                if (Parsed > 0)
                {
                    AccumTargetFrames = static_cast<uint32_t>(Parsed);
                }
            }
            catch (...) {}
        }
        HLVM_LOG(LogTest, info, TXT("Accumulating {} path-tracing frames (set HLVM_PT_ACCUM_FRAMES to override)"), AccumTargetFrames);

        bDumpRequested = (std::getenv("HLVM_DUMP_PTGI") != nullptr);
        if (bDumpRequested)
        {
            HLVM_LOG(LogTest, info, TXT("Frame dumping enabled (set HLVM_DUMP_PTGI to dump PNGs)"));
        }

        // Traditional CPU reference render: validates scene geometry, materials,
        // camera and light with zero GPU ray-tracing involvement.
        if (bDumpRequested || std::getenv("HLVM_PT_CPU_REF"))
        {
            RenderCPUReferenceAndDump();
        }

        // Default exposure 1.0 — with the payload/camera fixes the Cornell box
        // radiance sits in a sane range for ACES (the old 0.3 default was a
        // workaround for the saturated white-noise output of the broken build).
        // Override with HLVM_PT_EXPOSURE env var.
        Exposure = 1.0f;
        if (const char* EnvExposure = std::getenv("HLVM_PT_EXPOSURE"))
        {
            try
            {
                float Parsed = std::stof(EnvExposure);
                if (Parsed > 0.0f)
                {
                    Exposure = Parsed;
                }
            }
            catch (...) {}
        }
        HLVM_LOG(LogTest, info, TXT("PathTraceGI exposure: {} (set HLVM_PT_EXPOSURE to override)"), Exposure);

        CommandList = NvrhiDevice->createCommandList();

        HLVM_LOG(LogTest, info, TXT("FPathTraceGIPass initialized successfully"));
        return true;
    }

    void Shutdown()
    {
        BindingCache.Clear();

        GIPass.Shutdown();

        CommandList = nullptr;
        OutputTexture = nullptr;
        AccumTexture = nullptr;
        DisplayTexture = nullptr;
        StagingTexture = nullptr;
        AccumulatePipeline = nullptr;
        AccumulateBindingLayout = nullptr;
        AccumulateCS = nullptr;
        AccumulateConstants = nullptr;
        GBufferWorldPos = nullptr;
        GBufferNormal = nullptr;
        GBufferMaterial = nullptr;
        ViewConstants = nullptr;
        LinearSampler = nullptr;
        SceneTLAS = nullptr;
        SceneBLASes.clear();
        VertexBuffer = nullptr;
        IndexBuffer = nullptr;
        InstanceInfoBuffer = nullptr;
        CPUTriangles.clear();
        Scene.reset();
    }

    virtual void Animate(float fElapsedTimeSeconds) override
    {
        FrameCount++;
        FPSUpdateTimer += fElapsedTimeSeconds;
        if (FPSUpdateTimer >= 1.0f)
        {
            float FPS = float(FrameCount) / FPSUpdateTimer;
            WindowTitle = FString::Format(
                TXT("Path Trace GI - FPS: {:.1f} | Accum: {}/{}"), FPS, AccumFrameCount, AccumTargetFrames);
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
        if (CurrentFBInfo.width != LastWidth || CurrentFBInfo.height != LastHeight)
        {
            LastWidth = CurrentFBInfo.width;
            LastHeight = CurrentFBInfo.height;
            BindingCache.Clear();
        }

        UpdateViewConstants(CurrentFBInfo.width, CurrentFBInfo.height);
        FillGBufferTextures(CurrentFBInfo.width, CurrentFBInfo.height);

        CommandList->open();

        GI::FGIPassDesc Desc{};
        Desc.GBufferWorldPos = GBufferWorldPos;
        Desc.GBufferNormal = GBufferNormal;
        Desc.GBufferMaterial = GBufferMaterial;
        Desc.LinearSampler = LinearSampler;
        Desc.ViewConstants = ViewConstants;
        Desc.SceneTLAS = SceneTLAS;
        Desc.OutputTexture = OutputTexture;
        Desc.RTVertices = VertexBuffer;
        Desc.RTIndices = IndexBuffer;
        Desc.RTInstanceInfo = InstanceInfoBuffer;
        Desc.OutputWidth = CurrentFBInfo.width;
        Desc.OutputHeight = CurrentFBInfo.height;
        Desc.MaxBounces = 3;
        Desc.SamplesPerPixel = 4;
        Desc.EnableRR = true;
        Desc.FrameIndex = FrameIndex++;
        // The Cornell box is a closed room lit only by the ceiling light: the
        // fake constant ambient term would wash out the GI result (indirect
        // light comes from the path tracer's own bounces instead).
        Desc.AmbientScale = 0.0f;

        GIPass.DispatchRays(CommandList, Desc);

        // Temporal accumulation: OutputTexture (tonemapped per-sample radiance) ->
        // AccumTexture (running sum) -> DisplayTexture (averaged result).
        CommandList->setTextureState(
            OutputTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CommandList->setTextureState(
            AccumTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
        CommandList->setTextureState(
            DisplayTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

        ++AccumFrameCount;
        const uint32_t FrameCountForShader = AccumFrameCount; // 1-based
        struct FAccumConstants
        {
            uint32_t FrameCount;
            uint32_t Width;
            uint32_t Height;
            float    Exposure;
        };
        FAccumConstants AccumConstantsData;
        AccumConstantsData.FrameCount = FrameCountForShader;
        AccumConstantsData.Width = CurrentFBInfo.width;
        AccumConstantsData.Height = CurrentFBInfo.height;
        AccumConstantsData.Exposure = Exposure;
        CommandList->writeBuffer(AccumulateConstants, &AccumConstantsData, sizeof(AccumConstantsData));

        FBindingSetBuilder SetBuilder;
        SetBuilder.SetConstantBuffer(0, AccumulateConstants)
                  .SetTextureSRV(0, OutputTexture)
                  .SetTextureUAV(0, AccumTexture)
                  .SetTextureUAV(1, DisplayTexture);
        nvrhi::BindingSetHandle AccumBindingSet = NvrhiDevice->createBindingSet(SetBuilder.Build(), AccumulateBindingLayout);

        nvrhi::ComputeState AccumState;
        AccumState.setPipeline(AccumulatePipeline);
        AccumState.addBindingSet(AccumBindingSet);
        CommandList->setComputeState(AccumState);

        const uint32_t dispatchX = (CurrentFBInfo.width + 7) / 8;
        const uint32_t dispatchY = (CurrentFBInfo.height + 7) / 8;
        CommandList->dispatch(dispatchX, dispatchY, 1);

        // Blit the tonemapped accumulation result to the swapchain.
        CommandList->setTextureState(
            DisplayTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

        // Clear (and transition) the swapchain framebuffer before blitting.
        // Several other tests do this; without the clear the back-buffer can stay
        // in the Present layout and the blit may not actually write pixels.
        nvrhi::Color swapchainClearBlack(0.f, 0.f, 0.f, 1.f);
        nvrhi::utils::ClearColorAttachment(CommandList, Framebuffer, 0, swapchainClearBlack);

        FCommonRenderPasses::BlitParameters BlitParams;
        FCommonRenderPasses::BlitTexture(
            CommandList,
            Framebuffer,
            DisplayTexture,
            &BindingCache,
            CurrentFBInfo.width,
            CurrentFBInfo.height,
            BlitParams);

        CommandList->close();
        NvrhiDevice->executeCommandList(CommandList);

        const bool bIsLastFrame = (AccumFrameCount >= AccumTargetFrames);
        if (bDumpRequested && bIsLastFrame)
        {
            DumpFrameTextures(Framebuffer);
        }

        if (bIsLastFrame)
        {
            if (auto* DM = GetDeviceManager())
            {
                DM->StopMessageLoop();
            }
        }
    }

    virtual void BackBufferResizing() override
    {
        BindingCache.Clear();
    }

    uint32_t GetAccumTargetFrames() const { return AccumTargetFrames; }

private:
    bool CreateSceneGeometry()
    {
        const auto DataDir = FString::Format(
            TXT("{}/../../Test/{}_Data"), *GExecutablePath, *GExecutableName);
        const FPath LightsPath = FPath::Combine(DataDir, TXT("CornellBox_Lights.json"));

        Scene = FCornellBoxScene::Build(LightsPath);
        if (!Scene || !Scene->SceneNode || Scene->SceneNode->MeshTree.empty())
        {
            HLVM_LOG(LogTest, err, TXT("Failed to build Cornell box scene"));
            return false;
        }

        nvrhi::CommandListHandle InitCmd = NvrhiDevice->createCommandList();
        InitCmd->open();

        std::vector<FRTVertex> AllVertices;
        std::vector<uint32_t>  AllIndices;
        std::vector<FInstanceInfo> InstanceInfos;

        uint32_t CurrentVertexOffset = 0;
        uint32_t CurrentIndexOffset = 0;

        for (const auto& Entry : Scene->SceneNode->MeshTree)
        {
            auto StaticMesh = std::dynamic_pointer_cast<FStaticMesh>(Entry.second);
            if (!StaticMesh)
                continue;

            const auto& Verts = StaticMesh->GetVertices();
            const auto& Indices = StaticMesh->GetIndices();
            if (Verts.empty() || Indices.empty())
                continue;

            FInstanceInfo Info{};
            Info.VertexOffset = CurrentVertexOffset;
            Info.IndexOffset = CurrentIndexOffset;
            Info.VertexCount = static_cast<uint32_t>(Verts.size());
            Info.IndexCount = static_cast<uint32_t>(Indices.size());

            auto MatIt = Scene->SceneNode->MeshMultiMaterialMap.find(Entry.second);
            if (MatIt != Scene->SceneNode->MeshMultiMaterialMap.end() && !MatIt->second.empty())
            {
                const auto& Mat = MatIt->second[0];
                Info.AlbedoColor[0] = Mat->AlbedoColor.x;
                Info.AlbedoColor[1] = Mat->AlbedoColor.y;
                Info.AlbedoColor[2] = Mat->AlbedoColor.z;
            }
            else
            {
                Info.AlbedoColor[0] = 0.85f;
                Info.AlbedoColor[1] = 0.85f;
                Info.AlbedoColor[2] = 0.85f;
            }
            Info.AlbedoTextureIndex = 0;
            InstanceInfos.push_back(Info);

            for (const auto& V : Verts)
            {
                FRTVertex RTV;
                RTV.Position[0] = V.Position.x;
                RTV.Position[1] = V.Position.y;
                RTV.Position[2] = V.Position.z;
                RTV.Normal[0] = V.Normal.x;
                RTV.Normal[1] = V.Normal.y;
                RTV.Normal[2] = V.Normal.z;
                RTV.UV[0] = V.UV.x;
                RTV.UV[1] = V.UV.y;
                AllVertices.push_back(RTV);
            }

            for (uint32_t Idx : Indices)
            {
                AllIndices.push_back(Idx);
            }

            CurrentVertexOffset += Info.VertexCount;
            CurrentIndexOffset += Info.IndexCount;
        }

        if (AllVertices.empty() || AllIndices.empty())
        {
            HLVM_LOG(LogTest, err, TXT("Cornell box scene produced no geometry"));
            return false;
        }

        // Global vertex buffer
        {
            nvrhi::BufferDesc Desc;
            Desc.byteSize = static_cast<uint32_t>(AllVertices.size() * sizeof(FRTVertex));
            Desc.structStride = sizeof(FRTVertex);
            Desc.initialState = nvrhi::ResourceStates::ShaderResource;
            Desc.keepInitialState = true;
            Desc.isAccelStructBuildInput = true;
            Desc.debugName = "PathTraceVertices";
            VertexBuffer = NvrhiDevice->createBuffer(Desc);
            InitCmd->writeBuffer(VertexBuffer, AllVertices.data(), Desc.byteSize);
        }

        // Global index buffer
        {
            nvrhi::BufferDesc Desc;
            Desc.byteSize = static_cast<uint32_t>(AllIndices.size() * sizeof(uint32_t));
            Desc.structStride = sizeof(uint32_t);
            Desc.initialState = nvrhi::ResourceStates::ShaderResource;
            Desc.keepInitialState = true;
            Desc.isAccelStructBuildInput = true;
            Desc.debugName = "PathTraceIndices";
            IndexBuffer = NvrhiDevice->createBuffer(Desc);
            InitCmd->writeBuffer(IndexBuffer, AllIndices.data(), Desc.byteSize);
        }

        // Instance info buffer
        {
            nvrhi::BufferDesc Desc;
            Desc.byteSize = static_cast<uint32_t>(InstanceInfos.size() * sizeof(FInstanceInfo));
            Desc.structStride = sizeof(FInstanceInfo);
            Desc.initialState = nvrhi::ResourceStates::ShaderResource;
            Desc.keepInitialState = true;
            Desc.debugName = "PathTraceInstanceInfo";
            InstanceInfoBuffer = NvrhiDevice->createBuffer(Desc);
            InitCmd->writeBuffer(InstanceInfoBuffer, InstanceInfos.data(), Desc.byteSize);
        }

        // One BLAS per instance; all BLASes reference the same global buffers.
        SceneBLASes.clear();
        for (const FInstanceInfo& Info : InstanceInfos)
        {
            nvrhi::rt::AccelStructDesc BlasDesc;
            BlasDesc.isTopLevel = false;

            nvrhi::rt::GeometryDesc GeometryDesc;
            GeometryDesc.geometryType = nvrhi::rt::GeometryType::Triangles;
            GeometryDesc.flags = nvrhi::rt::GeometryFlags::Opaque;
            auto& Triangles = GeometryDesc.geometryData.triangles;
            Triangles.indexBuffer = IndexBuffer;
            Triangles.vertexBuffer = VertexBuffer;
            Triangles.indexFormat = nvrhi::Format::R32_UINT;
            Triangles.indexOffset = static_cast<uint64_t>(Info.IndexOffset) * sizeof(uint32_t);
            Triangles.indexCount = Info.IndexCount;
            Triangles.vertexFormat = nvrhi::Format::RGB32_FLOAT;
            Triangles.vertexStride = sizeof(FRTVertex);
            Triangles.vertexOffset = static_cast<uint64_t>(Info.VertexOffset) * sizeof(FRTVertex);
            Triangles.vertexCount = Info.VertexCount;
            BlasDesc.bottomLevelGeometries.push_back(GeometryDesc);

            nvrhi::rt::AccelStructHandle BLAS = NvrhiDevice->createAccelStruct(BlasDesc);
            nvrhi::utils::BuildBottomLevelAccelStruct(InitCmd, BLAS, BlasDesc);
            SceneBLASes.push_back(BLAS);
        }

        // TLAS
        {
            nvrhi::rt::AccelStructDesc TlasDesc;
            TlasDesc.isTopLevel = true;
            TlasDesc.topLevelMaxInstances = static_cast<uint32_t>(SceneBLASes.size());
            SceneTLAS = NvrhiDevice->createAccelStruct(TlasDesc);

            float Transform[12] = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f
            };

            std::vector<nvrhi::rt::InstanceDesc> InstanceDescs;
            InstanceDescs.reserve(SceneBLASes.size());
            for (size_t i = 0; i < SceneBLASes.size(); ++i)
            {
                nvrhi::rt::InstanceDesc InstanceDesc{};
                InstanceDesc.bottomLevelAS = SceneBLASes[i];
                InstanceDesc.instanceMask = 1;
                InstanceDesc.flags = nvrhi::rt::InstanceFlags::TriangleFrontCounterclockwise;
                memcpy(InstanceDesc.transform, Transform, sizeof(Transform));
                InstanceDescs.push_back(InstanceDesc);
            }

            InitCmd->buildTopLevelAccelStruct(SceneTLAS, InstanceDescs.data(), static_cast<uint32_t>(InstanceDescs.size()));
        }

        // Return geometry buffers to SRV state so the RT shaders can read them.
        InitCmd->setBufferState(VertexBuffer, nvrhi::ResourceStates::ShaderResource);
        InitCmd->setBufferState(IndexBuffer, nvrhi::ResourceStates::ShaderResource);

        InitCmd->close();
        NvrhiDevice->executeCommandList(InitCmd);
        NvrhiDevice->waitForIdle();

        BuildCPUTriangles(InstanceInfos, AllVertices, AllIndices);
        return true;
    }

    void BuildCPUTriangles(
        const std::vector<FInstanceInfo>& InstanceInfos,
        const std::vector<FRTVertex>& Vertices,
        const std::vector<uint32_t>& Indices)
    {
        CPUTriangles.clear();
        for (const FInstanceInfo& Info : InstanceInfos)
        {
            const glm::vec3 Albedo(Info.AlbedoColor[0], Info.AlbedoColor[1], Info.AlbedoColor[2]);
            const uint32_t EndIndex = Info.IndexOffset + Info.IndexCount;
            for (uint32_t i = Info.IndexOffset; i + 2 < EndIndex; i += 3)
            {
                const FRTVertex& A = Vertices[Info.VertexOffset + Indices[i + 0]];
                const FRTVertex& B = Vertices[Info.VertexOffset + Indices[i + 1]];
                const FRTVertex& C = Vertices[Info.VertexOffset + Indices[i + 2]];

                FCPUTriangle Tri;
                Tri.V0 = glm::vec3(A.Position[0], A.Position[1], A.Position[2]);
                Tri.V1 = glm::vec3(B.Position[0], B.Position[1], B.Position[2]);
                Tri.V2 = glm::vec3(C.Position[0], C.Position[1], C.Position[2]);
                // Use the mesh vertex normals (constant per quad): the left/right
                // walls are wound so their geometric cross-product normal points
                // OUT of the box, while shading needs the inward-facing normal.
                Tri.Normal = glm::normalize(
                    glm::vec3(A.Normal[0], A.Normal[1], A.Normal[2]) +
                    glm::vec3(B.Normal[0], B.Normal[1], B.Normal[2]) +
                    glm::vec3(C.Normal[0], C.Normal[1], C.Normal[2]));
                Tri.Albedo = Albedo;
                CPUTriangles.push_back(Tri);
            }
        }
    }

    bool CreateGBufferTextures()
    {
        GBufferWorldPos = CreateTexture2D(
            NvrhiDevice, WIDTH, HEIGHT, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "PathTraceGBufferWorldPos");
        GBufferNormal = CreateTexture2D(
            NvrhiDevice, WIDTH, HEIGHT, nvrhi::Format::RGBA8_UNORM,
            nvrhi::ResourceStates::UnorderedAccess, "PathTraceGBufferNormal");
        GBufferMaterial = CreateTexture2D(
            NvrhiDevice, WIDTH, HEIGHT, nvrhi::Format::RGBA8_UNORM,
            nvrhi::ResourceStates::UnorderedAccess, "PathTraceGBufferMaterial");
        return true;
    }

    bool CreateAccumulationPipeline(const FString& ShaderDataDir)
    {
        const std::string SblobPath = FPath::Combine(ShaderDataDir, TXT("GIAccumulate_cs.sblob")).string();
        auto Blob = ReadBinaryFile(SblobPath);
        if (Blob.empty())
        {
            HLVM_LOG(LogTest, err, TXT("Failed to read GIAccumulate_cs.sblob at %s"), *FString(SblobPath.c_str()));
            return false;
        }

        const void* ShaderBinary = nullptr;
        size_t      ShaderBinarySize = 0;
        if (!ShaderMake::FindPermutationInBlob(Blob.data(), Blob.size(), nullptr, 0, &ShaderBinary, &ShaderBinarySize))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to extract GIAccumulate_cs from blob"));
            return false;
        }

        nvrhi::ShaderDesc CSDesc;
        CSDesc.setShaderType(nvrhi::ShaderType::Compute);
        AccumulateCS = NvrhiDevice->createShader(CSDesc, ShaderBinary, ShaderBinarySize);
        if (!AccumulateCS)
        {
            HLVM_LOG(LogTest, err, TXT("Failed to create GIAccumulate_cs shader"));
            return false;
        }

        FBindingLayoutBuilder LayoutBuilder;
        LayoutBuilder.SetVisibility(nvrhi::ShaderType::Compute)
                     .AddConstantBuffer(0)     // b0 : AccumConstants
                     .AddTextureSRV(0)         // t0 : InputTexture
                     .AddTextureUAV(0)         // u0 : AccumTexture
                     .AddTextureUAV(1);        // u1 : DisplayTexture
        AccumulateBindingLayout = NvrhiDevice->createBindingLayout(LayoutBuilder.Build());
        if (!AccumulateBindingLayout)
        {
            HLVM_LOG(LogTest, err, TXT("Failed to create accumulation binding layout"));
            return false;
        }

        nvrhi::ComputePipelineDesc PipelineDesc;
        PipelineDesc.setComputeShader(AccumulateCS);
        PipelineDesc.addBindingLayout(AccumulateBindingLayout);
        AccumulatePipeline = NvrhiDevice->createComputePipeline(PipelineDesc);
        if (!AccumulatePipeline)
        {
            HLVM_LOG(LogTest, err, TXT("Failed to create accumulation compute pipeline"));
            return false;
        }

        nvrhi::BufferDesc CBDesc;
        CBDesc.byteSize = sizeof(uint32_t) * 4;
        CBDesc.isConstantBuffer = true;
        CBDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
        CBDesc.keepInitialState = true;
        CBDesc.debugName = "PathTraceAccumConstants";
        AccumulateConstants = NvrhiDevice->createBuffer(CBDesc);
        if (!AccumulateConstants)
        {
            HLVM_LOG(LogTest, err, TXT("Failed to create accumulation constants buffer"));
            return false;
        }

        return true;
    }

    void UpdateViewConstants(uint32_t W, uint32_t H)
    {
        FViewConstants Constants{};

        // Identity model
        for (int i = 0; i < 4; ++i)
            Constants.Model[i][i] = 1.0f;

        // Shared Cornell-box camera (inside the box near the front wall)
        const FCameraRig Rig = GetCameraRig();
        glm::mat4 View = GetCameraView(Rig);
        glm::mat4 Proj = GetCameraProj(Rig, W, H);

        memcpy(Constants.View, glm::value_ptr(View), sizeof(Constants.View));
        memcpy(Constants.Proj, glm::value_ptr(Proj), sizeof(Constants.Proj));
        Constants.RenderTargetSize[0] = float(W);
        Constants.RenderTargetSize[1] = float(H);
        Constants.FrameIndex = float(FrameIndex);

        nvrhi::CommandListHandle Cmd = NvrhiDevice->createCommandList();
        Cmd->open();
        Cmd->writeBuffer(ViewConstants, &Constants, sizeof(Constants));
        Cmd->close();
        NvrhiDevice->executeCommandList(Cmd);
    }

    static bool IntersectRayTriangle(
        const glm::vec3& Origin,
        const glm::vec3& Dir,
        const FCPUTriangle& Tri,
        float& OutT)
    {
        const float EPS = 1e-4f;
        const glm::vec3 Edge1 = Tri.V1 - Tri.V0;
        const glm::vec3 Edge2 = Tri.V2 - Tri.V0;
        const glm::vec3 H = glm::cross(Dir, Edge2);
        const float A = glm::dot(Edge1, H);
        if (std::fabs(A) < EPS)
            return false;

        const float F = 1.0f / A;
        const glm::vec3 S = Origin - Tri.V0;
        const float U = F * glm::dot(S, H);
        if (U < 0.0f || U > 1.0f)
            return false;

        const glm::vec3 Q = glm::cross(S, Edge1);
        const float V = F * glm::dot(Dir, Q);
        if (V < 0.0f || U + V > 1.0f)
            return false;

        const float T = F * glm::dot(Edge2, Q);
        if (T > EPS)
        {
            OutT = T;
            return true;
        }
        return false;
    }

    void FillGBufferTextures(uint32_t W, uint32_t H)
    {
        // The camera is static, so the CPU GBuffer only needs to be filled once.
        if (bGBufferFilled)
            return;
        bGBufferFilled = true;

        std::vector<float> WorldPosData(size_t(W) * H * 4);
        std::vector<uint8_t> NormalData(size_t(W) * H * 4);
        std::vector<uint32_t> MaterialData(size_t(W) * H);

        const FCameraRig Rig = GetCameraRig();
        const glm::vec3 CameraPos = Rig.Position;
        glm::mat4 View = GetCameraView(Rig);
        glm::mat4 Proj = GetCameraProj(Rig, W, H);
        glm::mat4 InvProj = glm::inverse(Proj);
        glm::mat4 InvView = glm::inverse(View);

        for (uint32_t y = 0; y < H; ++y)
        {
            for (uint32_t x = 0; x < W; ++x)
            {
                size_t idx = size_t(y) * W + x;

                glm::vec3 RayDir = MakeCameraRay(x, y, W, H, InvView, InvProj);

                float ClosestT = std::numeric_limits<float>::max();
                const FCPUTriangle* HitTri = nullptr;

                for (const FCPUTriangle& Tri : CPUTriangles)
                {
                    float T = 0.0f;
                    if (IntersectRayTriangle(CameraPos, RayDir, Tri, T) && T < ClosestT)
                    {
                        ClosestT = T;
                        HitTri = &Tri;
                    }
                }

                if (HitTri)
                {
                    glm::vec3 HitPos = CameraPos + RayDir * ClosestT;
                    WorldPosData[idx * 4 + 0] = HitPos.x;
                    WorldPosData[idx * 4 + 1] = HitPos.y;
                    WorldPosData[idx * 4 + 2] = HitPos.z;
                    WorldPosData[idx * 4 + 3] = 1.0f;

                    glm::vec3 EncodedNormal = HitTri->Normal * 0.5f + 0.5f;
                    NormalData[idx * 4 + 0] = static_cast<uint8_t>(EncodedNormal.x * 255.0f);
                    NormalData[idx * 4 + 1] = static_cast<uint8_t>(EncodedNormal.y * 255.0f);
                    NormalData[idx * 4 + 2] = static_cast<uint8_t>(EncodedNormal.z * 255.0f);
                    NormalData[idx * 4 + 3] = 0;

                    uint8_t R = static_cast<uint8_t>(glm::clamp(HitTri->Albedo.r, 0.0f, 1.0f) * 255.0f);
                    uint8_t G = static_cast<uint8_t>(glm::clamp(HitTri->Albedo.g, 0.0f, 1.0f) * 255.0f);
                    uint8_t B = static_cast<uint8_t>(glm::clamp(HitTri->Albedo.b, 0.0f, 1.0f) * 255.0f);
                    uint8_t A = 255;
                    MaterialData[idx] = static_cast<uint32_t>((A << 24) | (B << 16) | (G << 8) | R);
                }
                else
                {
                    WorldPosData[idx * 4 + 0] = 0.0f;
                    WorldPosData[idx * 4 + 1] = 0.0f;
                    WorldPosData[idx * 4 + 2] = 0.0f;
                    WorldPosData[idx * 4 + 3] = 0.0f;

                    NormalData[idx * 4 + 0] = 128;
                    NormalData[idx * 4 + 1] = 128;
                    NormalData[idx * 4 + 2] = 128;
                    NormalData[idx * 4 + 3] = 0;

                    MaterialData[idx] = 0;
                }
            }
        }

        nvrhi::CommandListHandle Cmd = NvrhiDevice->createCommandList();
        Cmd->open();

        Cmd->setTextureState(GBufferWorldPos, nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
        Cmd->setTextureState(GBufferNormal, nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
        Cmd->setTextureState(GBufferMaterial, nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);

        Cmd->writeTexture(GBufferWorldPos, 0, 0, WorldPosData.data(), W * sizeof(float) * 4);
        Cmd->writeTexture(GBufferNormal, 0, 0, NormalData.data(), W * sizeof(uint8_t) * 4);
        Cmd->writeTexture(GBufferMaterial, 0, 0, MaterialData.data(), W * sizeof(uint32_t));

        Cmd->setTextureState(GBufferWorldPos, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        Cmd->setTextureState(GBufferNormal, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        Cmd->setTextureState(GBufferMaterial, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

        Cmd->close();
        NvrhiDevice->executeCommandList(Cmd);
    }

    // ------------------------------------------------------------------
    // Traditional CPU reference render. Validates scene geometry, materials,
    // camera and light with zero GPU ray-tracing involvement: primary rays +
    // direct lighting from the area light (stratified sampling + CPU shadow
    // rays). Dumps CPUReference_{Direct,Albedo,Normal}.png for comparison
    // against the GPU path-traced result.
    // ------------------------------------------------------------------
    bool TraceCPUScene(const glm::vec3& Origin, const glm::vec3& Dir,
                       float& OutT, const FCPUTriangle*& OutTri) const
    {
        float ClosestT = std::numeric_limits<float>::max();
        const FCPUTriangle* HitTri = nullptr;
        for (const FCPUTriangle& Tri : CPUTriangles)
        {
            float T = 0.0f;
            if (IntersectRayTriangle(Origin, Dir, Tri, T) && T < ClosestT)
            {
                ClosestT = T;
                HitTri = &Tri;
            }
        }
        if (HitTri)
        {
            OutT = ClosestT;
            OutTri = HitTri;
            return true;
        }
        return false;
    }

    static glm::vec3 ACESFilm(const glm::vec3& x)
    {
        const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
        return glm::clamp((x * (a * x + b)) / (x * (c * x + d) + e),
                          glm::vec3(0.0f), glm::vec3(1.0f));
    }

    void RenderCPUReferenceAndDump()
    {
        if (CPUTriangles.empty() || !Scene)
            return;

        // Find the first area light in the scene.
        const Renderer::FLight* AreaLight = nullptr;
        for (const auto& L : Scene->Lights)
        {
            if (L.type == static_cast<uint32_t>(Renderer::ELightType::Area))
            {
                AreaLight = &L;
                break;
            }
        }
        if (!AreaLight)
        {
            HLVM_LOG(LogTest, warn, TXT("CPU reference: no area light found, skipping"));
            return;
        }

        const FCameraRig Rig = GetCameraRig();
        const glm::vec3 CameraPos = Rig.Position;
        const glm::mat4 InvView = glm::inverse(GetCameraView(Rig));
        const glm::mat4 InvProj = glm::inverse(GetCameraProj(Rig, WIDTH, HEIGHT));

        const glm::vec3 LightPos(AreaLight->position[0], AreaLight->position[1], AreaLight->position[2]);
        const glm::vec3 LightNormal = glm::normalize(glm::vec3(
            AreaLight->direction[0], AreaLight->direction[1], AreaLight->direction[2]));
        const glm::vec3 LightColor = glm::vec3(
            AreaLight->color[0], AreaLight->color[1], AreaLight->color[2]) * AreaLight->intensity;
        const float LightArea = AreaLight->areaWidth * AreaLight->areaHeight;
        const glm::vec3 UpRef = (std::fabs(LightNormal.y) < 0.999f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        const glm::vec3 LightTangent = glm::normalize(glm::cross(UpRef, LightNormal));
        const glm::vec3 LightBitangent = glm::cross(LightNormal, LightTangent);

        constexpr uint32_t LIGHT_SAMPLES = 8; // 8x8 stratified over the light
        constexpr float kPi = 3.14159265f;

        std::vector<float> DirectData(size_t(WIDTH) * HEIGHT * 4);
        std::vector<float> AlbedoData(size_t(WIDTH) * HEIGHT * 4);
        std::vector<float> NormalData(size_t(WIDTH) * HEIGHT * 4);

        for (uint32_t y = 0; y < HEIGHT; ++y)
        {
            for (uint32_t x = 0; x < WIDTH; ++x)
            {
                const size_t Idx = (size_t(y) * WIDTH + x) * 4;
                const glm::vec3 RayDir = MakeCameraRay(x, y, WIDTH, HEIGHT, InvView, InvProj);

                float HitT = 0.0f;
                const FCPUTriangle* HitTri = nullptr;
                if (!TraceCPUScene(CameraPos, RayDir, HitT, HitTri))
                {
                    DirectData[Idx + 3] = 1.0f;
                    AlbedoData[Idx + 3] = 1.0f;
                    NormalData[Idx + 3] = 1.0f;
                    continue;
                }

                const glm::vec3 HitPos = CameraPos + RayDir * HitT;
                const glm::vec3 N = HitTri->Normal;
                const glm::vec3 Albedo = HitTri->Albedo;

                AlbedoData[Idx + 0] = Albedo.r;
                AlbedoData[Idx + 1] = Albedo.g;
                AlbedoData[Idx + 2] = Albedo.b;
                AlbedoData[Idx + 3] = 1.0f;
                NormalData[Idx + 0] = N.x * 0.5f + 0.5f;
                NormalData[Idx + 1] = N.y * 0.5f + 0.5f;
                NormalData[Idx + 2] = N.z * 0.5f + 0.5f;
                NormalData[Idx + 3] = 1.0f;

                // Direct lighting: stratified sampling of the area light.
                glm::vec3 Direct(0.0f);
                for (uint32_t sy = 0; sy < LIGHT_SAMPLES; ++sy)
                {
                    for (uint32_t sx = 0; sx < LIGHT_SAMPLES; ++sx)
                    {
                        const float u = (float(sx) + 0.5f) / float(LIGHT_SAMPLES) - 0.5f;
                        const float v = (float(sy) + 0.5f) / float(LIGHT_SAMPLES) - 0.5f;
                        const glm::vec3 SamplePos = LightPos
                            + LightTangent * (u * AreaLight->areaWidth)
                            + LightBitangent * (v * AreaLight->areaHeight);

                        glm::vec3 ToLight = SamplePos - HitPos;
                        const float R2 = glm::dot(ToLight, ToLight);
                        const float R = std::sqrt(R2);
                        ToLight /= R;

                        const float CosLight = glm::dot(-ToLight, LightNormal);
                        if (CosLight <= 0.0f)
                            continue;
                        const float NdotL = glm::dot(N, ToLight);
                        if (NdotL <= 0.0f)
                            continue;

                        // Shadow ray toward the light sample.
                        const glm::vec3 ShadowOrigin = HitPos + N * 0.001f;
                        bool Occluded = false;
                        for (const FCPUTriangle& Tri : CPUTriangles)
                        {
                            float T = 0.0f;
                            if (IntersectRayTriangle(ShadowOrigin, ToLight, Tri, T) && T < R - 0.002f)
                            {
                                Occluded = true;
                                break;
                            }
                        }
                        if (Occluded)
                            continue;

                        const float Pdf = R2 / (LightArea * CosLight); // solid-angle pdf
                        Direct += Albedo * (NdotL / kPi) * LightColor / Pdf;
                    }
                }
                Direct /= float(LIGHT_SAMPLES * LIGHT_SAMPLES);

                const glm::vec3 Mapped = glm::pow(ACESFilm(Direct), glm::vec3(1.0f / 2.2f));
                DirectData[Idx + 0] = Mapped.r;
                DirectData[Idx + 1] = Mapped.g;
                DirectData[Idx + 2] = Mapped.b;
                DirectData[Idx + 3] = 1.0f;
            }
        }

        FString DumpDir = FString::Format(
            TXT("{}/Engine/Source/Runtime/Test/TestPathTraceGI_Data/dumps"),
            *GProjectRoot);
        std::filesystem::create_directories(FPath(DumpDir).string());

        FImageDump::DumpToPNG(FPath::Combine(DumpDir, TXT("CPUReference_Direct.png")).string(),
            static_cast<int>(WIDTH), static_cast<int>(HEIGHT), DirectData.data());
        FImageDump::DumpToPNG(FPath::Combine(DumpDir, TXT("CPUReference_Albedo.png")).string(),
            static_cast<int>(WIDTH), static_cast<int>(HEIGHT), AlbedoData.data());
        FImageDump::DumpToPNG(FPath::Combine(DumpDir, TXT("CPUReference_Normal.png")).string(),
            static_cast<int>(WIDTH), static_cast<int>(HEIGHT), NormalData.data());

        HLVM_LOG(LogTest, info, TXT("CPU reference render dumped (Direct/Albedo/Normal)"));
    }

public:
    // Read back the accumulated/tonemapped display texture and compute mean RGB luminance.
    // Returns negative value on failure.
    float ComputeOutputMeanLuminance()
    {
        if (!NvrhiDevice || !DisplayTexture || !StagingTexture)
            return -1.0f;

        nvrhi::CommandListHandle Cmd = NvrhiDevice->createCommandList();
        Cmd->open();
        Cmd->setTextureState(DisplayTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopySource);

        nvrhi::TextureSlice Slice = {};
        Slice.width = WIDTH;
        Slice.height = HEIGHT;
        Slice.depth = 1;
        Cmd->copyTexture(StagingTexture, Slice, DisplayTexture, Slice);

        Cmd->close();
        NvrhiDevice->executeCommandList(Cmd);
        NvrhiDevice->waitForIdle();

        size_t RowPitch = 0;
        void* MappedData = NvrhiDevice->mapStagingTexture(
            StagingTexture, Slice, nvrhi::CpuAccessMode::Read, &RowPitch);
        if (!MappedData)
            return -2.0f;

        double TotalLuma = 0.0;
        uint32_t ValidPixels = 0;
        const uint8_t* SrcRow = reinterpret_cast<const uint8_t*>(MappedData);
        for (uint32_t y = 0; y < HEIGHT; ++y)
        {
            const float* Src = reinterpret_cast<const float*>(SrcRow + static_cast<size_t>(y) * RowPitch);
            for (uint32_t x = 0; x < WIDTH; ++x)
            {
                size_t idx = x * 4;
                float r = Src[idx + 0];
                float g = Src[idx + 1];
                float b = Src[idx + 2];
                // Output is already tonemapped; luma is post-tonemap.
                TotalLuma += static_cast<double>(r * 0.2126f + g * 0.7152f + b * 0.0722f);
                ++ValidPixels;
            }
        }
        NvrhiDevice->unmapStagingTexture(StagingTexture);

        if (ValidPixels == 0)
            return -3.0f;
        return static_cast<float>(TotalLuma / static_cast<double>(ValidPixels));
    }

    // Dump intermediate and final textures to PNG for visual verification.
    // Set HLVM_DUMP_PTGI=1 to enable.
    void DumpFrameTextures(nvrhi::IFramebuffer* Framebuffer)
    {
        if (!NvrhiDevice)
            return;

        FString DumpDir = FString::Format(
            TXT("{}/Engine/Source/Runtime/Test/TestPathTraceGI_Data/dumps"),
            *GProjectRoot);
        std::filesystem::create_directories(FPath(DumpDir).string());

        DumpRGBA32FTexture(OutputTexture, TXT("Output"), DumpDir);
        DumpRGBA32FTexture(AccumTexture, TXT("Accum"), DumpDir);
        DumpRGBA32FTexture(DisplayTexture, TXT("Display"), DumpDir);

        if (Framebuffer)
            DumpBackbuffer(Framebuffer, DumpDir);
    }

private:
    void DumpRGBA32FTexture(nvrhi::TextureHandle Texture, const FString& Name, const FString& DumpDir)
    {
        if (!Texture)
            return;

        nvrhi::CommandListHandle Cmd = NvrhiDevice->createCommandList();
        Cmd->open();
        Cmd->setTextureState(Texture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopySource);

        nvrhi::TextureSlice Slice = {};
        Slice.width = WIDTH;
        Slice.height = HEIGHT;
        Slice.depth = 1;
        Cmd->copyTexture(StagingTexture, Slice, Texture, Slice);

        Cmd->close();
        NvrhiDevice->executeCommandList(Cmd);
        NvrhiDevice->waitForIdle();

        size_t RowPitch = 0;
        void* MappedData = NvrhiDevice->mapStagingTexture(
            StagingTexture, Slice, nvrhi::CpuAccessMode::Read, &RowPitch);
        if (!MappedData)
            return;

        std::vector<float> Pixels(static_cast<size_t>(WIDTH) * HEIGHT * 4);
        const uint8_t* SrcRow = reinterpret_cast<const uint8_t*>(MappedData);
        double MinR = 1e30, MinG = 1e30, MinB = 1e30;
        double MaxR = -1e30, MaxG = -1e30, MaxB = -1e30;
        double SumR = 0.0, SumG = 0.0, SumB = 0.0;
        uint32_t ValidPixels = 0;
        uint32_t SaturatedCount = 0;  // pixels with any channel > 1.0 (HDR overflow)
        uint32_t BlackCount = 0;      // pixels with all channels < 0.001 (no light)

        for (uint32_t y = 0; y < HEIGHT; ++y)
        {
            const float* Src = reinterpret_cast<const float*>(SrcRow + static_cast<size_t>(y) * RowPitch);
            for (uint32_t x = 0; x < WIDTH; ++x)
            {
                size_t SrcIdx = x * 4;
                float r = Src[SrcIdx + 0];
                float g = Src[SrcIdx + 1];
                float b = Src[SrcIdx + 2];

                size_t DstIdx = (static_cast<size_t>(y) * WIDTH + x) * 4;
                Pixels[DstIdx + 0] = r;
                Pixels[DstIdx + 1] = g;
                Pixels[DstIdx + 2] = b;
                Pixels[DstIdx + 3] = 1.0f;

                MinR = std::min(MinR, static_cast<double>(r));
                MinG = std::min(MinG, static_cast<double>(g));
                MinB = std::min(MinB, static_cast<double>(b));
                MaxR = std::max(MaxR, static_cast<double>(r));
                MaxG = std::max(MaxG, static_cast<double>(g));
                MaxB = std::max(MaxB, static_cast<double>(b));
                SumR += static_cast<double>(r);
                SumG += static_cast<double>(g);
                SumB += static_cast<double>(b);
                if (r > 1.0f || g > 1.0f || b > 1.0f) ++SaturatedCount;
                if (r < 0.001f && g < 0.001f && b < 0.001f) ++BlackCount;
                ++ValidPixels;
            }
        }
        NvrhiDevice->unmapStagingTexture(StagingTexture);

        if (ValidPixels > 0)
        {
            double Inv = 1.0 / static_cast<double>(ValidPixels);
            HLVM_LOG(LogTest, info,
                TXT("Dump [{}] frame {}: min({:.4f},{:.4f},{:.4f}) max({:.4f},{:.4f},{:.4f}) mean({:.4f},{:.4f},{:.4f}) sat%={:.1f} black%={:.1f}"),
                *Name, AccumFrameCount,
                MinR, MinG, MinB, MaxR, MaxG, MaxB,
                SumR * Inv, SumG * Inv, SumB * Inv,
                100.0 * static_cast<double>(SaturatedCount) / static_cast<double>(ValidPixels),
                100.0 * static_cast<double>(BlackCount) / static_cast<double>(ValidPixels));
        }

        FString Filename = FPath::Combine(DumpDir,
            FString::Format(TXT("{}_frame{:04d}.png"), *Name, AccumFrameCount)).string();
        if (FImageDump::DumpToPNG(Filename, static_cast<int>(WIDTH), static_cast<int>(HEIGHT), Pixels.data()))
        {
            HLVM_LOG(LogTest, info, TXT("Dumped {} to {}"), *Name, *Filename);
        }
        else
        {
            HLVM_LOG(LogTest, err, TXT("Failed to dump {} to {}"), *Name, *Filename);
        }
    }

    void DumpBackbuffer(nvrhi::IFramebuffer* Framebuffer, const FString& DumpDir)
    {
        if (!Framebuffer)
            return;

        const nvrhi::FramebufferDesc& Desc = Framebuffer->getDesc();
        if (Desc.colorAttachments.empty() || !Desc.colorAttachments[0].texture)
            return;

        nvrhi::TextureHandle Backbuffer = Desc.colorAttachments[0].texture;
        nvrhi::TextureDesc BBDesc = Backbuffer->getDesc();

        nvrhi::TextureDesc StagingDesc;
        StagingDesc.dimension = nvrhi::TextureDimension::Texture2D;
        StagingDesc.width = BBDesc.width;
        StagingDesc.height = BBDesc.height;
        StagingDesc.format = BBDesc.format;
        StagingDesc.initialState = nvrhi::ResourceStates::CopyDest;
        StagingDesc.keepInitialState = false;
        StagingDesc.debugName = "PathTraceBackbufferStaging";
        nvrhi::StagingTextureHandle BBStaging = NvrhiDevice->createStagingTexture(StagingDesc, nvrhi::CpuAccessMode::Read);
        if (!BBStaging)
            return;

        nvrhi::CommandListHandle Cmd = NvrhiDevice->createCommandList();
        Cmd->open();
        Cmd->setTextureState(Backbuffer, nvrhi::AllSubresources, nvrhi::ResourceStates::CopySource);

        nvrhi::TextureSlice Slice = {};
        Slice.width = BBDesc.width;
        Slice.height = BBDesc.height;
        Slice.depth = 1;
        Cmd->copyTexture(BBStaging, Slice, Backbuffer, Slice);

        Cmd->close();
        NvrhiDevice->executeCommandList(Cmd);
        NvrhiDevice->waitForIdle();

        size_t RowPitch = 0;
        void* MappedData = NvrhiDevice->mapStagingTexture(
            BBStaging, Slice, nvrhi::CpuAccessMode::Read, &RowPitch);
        if (!MappedData)
            return;

        std::vector<float> Pixels(static_cast<size_t>(BBDesc.width) * BBDesc.height * 4);
        const uint8_t* SrcRow = reinterpret_cast<const uint8_t*>(MappedData);
        double MinR = 1e30, MinG = 1e30, MinB = 1e30;
        double MaxR = -1e30, MaxG = -1e30, MaxB = -1e30;
        double SumR = 0.0, SumG = 0.0, SumB = 0.0;
        uint32_t ValidPixels = 0;

        for (uint32_t y = 0; y < BBDesc.height; ++y)
        {
            const uint8_t* Src = SrcRow + static_cast<size_t>(y) * RowPitch;
            for (uint32_t x = 0; x < BBDesc.width; ++x)
            {
                size_t SrcIdx = x * 4;
                // B8G8R8A8_UNORM byte order: B, G, R, A
                float b = Src[SrcIdx + 0] / 255.0f;
                float g = Src[SrcIdx + 1] / 255.0f;
                float r = Src[SrcIdx + 2] / 255.0f;

                size_t DstIdx = (static_cast<size_t>(y) * BBDesc.width + x) * 4;
                Pixels[DstIdx + 0] = r;
                Pixels[DstIdx + 1] = g;
                Pixels[DstIdx + 2] = b;
                Pixels[DstIdx + 3] = 1.0f;

                MinR = std::min(MinR, static_cast<double>(r));
                MinG = std::min(MinG, static_cast<double>(g));
                MinB = std::min(MinB, static_cast<double>(b));
                MaxR = std::max(MaxR, static_cast<double>(r));
                MaxG = std::max(MaxG, static_cast<double>(g));
                MaxB = std::max(MaxB, static_cast<double>(b));
                SumR += static_cast<double>(r);
                SumG += static_cast<double>(g);
                SumB += static_cast<double>(b);
                ++ValidPixels;
            }
        }
        NvrhiDevice->unmapStagingTexture(BBStaging);

        if (ValidPixels > 0)
        {
            double Inv = 1.0 / static_cast<double>(ValidPixels);
            HLVM_LOG(LogTest, info,
                TXT("Dump [Backbuffer] frame {}: min({:.4f},{:.4f},{:.4f}) max({:.4f},{:.4f},{:.4f}) mean({:.4f},{:.4f},{:.4f})"),
                AccumFrameCount,
                MinR, MinG, MinB, MaxR, MaxG, MaxB,
                SumR * Inv, SumG * Inv, SumB * Inv);
        }

        FString Filename = FPath::Combine(DumpDir,
            FString::Format(TXT("Backbuffer_frame{:04d}.png"), AccumFrameCount)).string();
        if (FImageDump::DumpToPNG(Filename, static_cast<int>(BBDesc.width), static_cast<int>(BBDesc.height), Pixels.data()))
        {
            HLVM_LOG(LogTest, info, TXT("Dumped Backbuffer to {}"), *Filename);
        }
        else
        {
            HLVM_LOG(LogTest, err, TXT("Failed to dump Backbuffer to {}"), *Filename);
        }
    }
    nvrhi::IDevice*          NvrhiDevice = nullptr;
    nvrhi::FramebufferInfo   FBInfo;
    FString                  WindowTitle;

    nvrhi::CommandListHandle CommandList;
    FBindingCache            BindingCache;

    nvrhi::SamplerHandle     LinearSampler;
    nvrhi::BufferHandle      ViewConstants;

    nvrhi::BufferHandle      VertexBuffer;
    nvrhi::BufferHandle      IndexBuffer;
    nvrhi::BufferHandle      InstanceInfoBuffer;
    std::vector<nvrhi::rt::AccelStructHandle> SceneBLASes;
    nvrhi::rt::AccelStructHandle SceneTLAS;

    nvrhi::TextureHandle     GBufferWorldPos;
    nvrhi::TextureHandle     GBufferNormal;
    nvrhi::TextureHandle     GBufferMaterial;
    nvrhi::TextureHandle     OutputTexture;
    nvrhi::TextureHandle     AccumTexture;
    nvrhi::TextureHandle     DisplayTexture;
    nvrhi::StagingTextureHandle StagingTexture;

    nvrhi::ShaderHandle      AccumulateCS;
    nvrhi::BindingLayoutHandle AccumulateBindingLayout;
    nvrhi::ComputePipelineHandle AccumulatePipeline;
    nvrhi::BufferHandle      AccumulateConstants;

    GI::FGIPass              GIPass;

    std::shared_ptr<FScene>  Scene;
    std::vector<FCPUTriangle> CPUTriangles;

    uint32_t LastWidth = 0;
    uint32_t LastHeight = 0;
    uint32_t FrameCount = 0;
    uint32_t FrameIndex = 0;
    uint32_t AccumFrameCount = 0;
    uint32_t AccumTargetFrames = DEFAULT_ACCUM_TARGET_FRAMES;
    bool     bDumpRequested = false;
    bool     bGBufferFilled = false;
    float    Exposure = 1.0f;
    float    FPSUpdateTimer = 0.0f;
};

// =============================================================================
// TEST IMPLEMENTATION
// =============================================================================

RECORD_BOOL(test_PathTraceGI)
{
    HLVM_LOG(LogTest, info, TXT("Starting Path Trace GI Test..."));

    try
    {
        IWindow::Properties WindowProps;
        WindowProps.Title = WINDOW_TITLE;
        WindowProps.Extent = { WIDTH, HEIGHT };
        WindowProps.Resizable = false;
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
        DeviceParams.MaxTimerQueries = 2048;

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

        TSharedPtr<FPathTraceGIPass> PathTracePass =
            std::make_shared<FPathTraceGIPass>(DeviceManager.get());
        if (!PathTracePass->Initialize(NvrhiDevice, FirstFB, FString(TXT("Path Trace GI Test"))))
        {
            throw std::runtime_error("Failed to initialize FPathTraceGIPass");
        }

        DeviceManager->AddRenderPassToBack(PathTracePass);

        HLVM_LOG(LogTest, info, TXT("Starting render loop..."));

        // Fallback safety timer: normally the render pass stops the loop once the
        // target accumulation count is reached. This timer prevents a runaway loop
        // if rendering is slower than expected. The timeout scales with the target
        // frame count so longer interactive accumulations (HLVM_PT_ACCUM_FRAMES)
        // are still allowed to complete.
        std::thread([&]() {
            const double TimeoutSec = std::max(30.0,
                static_cast<double>(PathTracePass->GetAccumTargetFrames()) * 2.0);
            FTimer Timer;
            while (Timer.MarkSec() < TimeoutSec)
            {
            }
            DeviceManager->StopMessageLoop();
        }).detach();

        DeviceManager->RunMessageLoop();

        // NEE quality gate: output must not be black (proves lights + NEE fired).
        float MeanLuma = PathTracePass->ComputeOutputMeanLuminance();
        HLVM_LOG(LogTest, info, TXT("PathTraceGI output mean luminance: {:.4f}"), MeanLuma);
        if (MeanLuma < 0.0f)
        {
            HLVM_LOG(LogTest, err, TXT("Failed to read back output texture (code {:.1f})"), MeanLuma);
            PathTracePass->Shutdown();
            return false;
        }
        if (MeanLuma < 0.01f)
        {
            HLVM_LOG(LogTest, err, TXT("Output too dark (mean luma {:.4f}); NEE may not be firing"), MeanLuma);
            PathTracePass->Shutdown();
            return false;
        }

        PathTracePass->Shutdown();

        HLVM_LOG(LogTest, info, TXT("Path Trace GI Test completed successfully!"));
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

RECORD_BOOL(test_PathTraceGI)
{
    HLVM_LOG(LogTest, warning, TXT("Vulkan renderer not enabled - skipping test"));
    return true;
}

#endif // HLVM_VULKAN_RENDERER
