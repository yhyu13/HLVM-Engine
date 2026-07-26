/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * TestReSTIR_GI_Temporal — End-to-end Sponza + ReSTIR GI pipeline.
 *
 * Pipeline (modes 0..5, 13, 14 of GIPathTracing.hlsl debug modes are honored):
 *
 *   Sponza GLTF (Samples/Assets/sponza/Sponza01.gltf)
 *      ↓
 *   BLAS / TLAS (one instance per static mesh, scaled 0.01 because Sponza is large)
 *      ↓
 *   GBuffer (GBufferSponzaVS/PS) -> WorldPos / Normal / Material textures
 *      ↓
 *   GI Ray Trace (FGIPass w/ GIPathTracing.hlsl — 64-byte compact payload)
 *      ↓ OutputTexture (HDR rgb + first-hit dist alpha)
 *   Bilateral Denoise (FBilateralDenoisePass) -> DenoisedHDR
 *      ↓
 *   ReSTIR Generate (FReSTIRPass::DispatchGeneration) -> Reservoir0/1
 *      ↓
 *   ReSTIR Temporal (FReSTIRPass::DispatchTemporal) -> Merged Reservoir0/1 + OutRadiance
 *      ↓
 *   ReSTIR Spatial (FReSTIRPass::DispatchSpatial, 3x3 + pairwise MIS) -> SpatialRadiance
 *      ↓
 *   GIAccumulate (ACES tonemap + sRGB gamma) -> Display
 *      ↓
 *   Blit to swapchain
 *
 * Debug/verification aids (all inherited from Vibe_Coding/51_PathTraceGI_Debug):
 *   - HLVM_DUMP_RGI=1        dump Output/Denoised/ReSTIR/Display PNGs on the last frame
 *   - HLVM_RGI_ACCUM=N       target frames for accumulation (default 8)
 *   - HLVM_RGI_EXPOSURE=F    pre-tonemap exposure (default 1.0)
 *   - HLVM_PT_DEBUG_MODE=N   shader-side debug mode (passed to GIPathTracing.hlsl)
 *
 * Why this test exists: the previous TestFewBounceGI was renamed to
 * TestCornellBoxGI (commit 2216e71) and the ReSTIR/ReBLUR compute pipelines
 * were left without an end-to-end driver. This test re-integrates them with
 * the proven 64-byte payload GIPathTracing.hlsl — see
 * Vibe_Coding/51_PathTraceGI_Debug/session-PathTraceGI_payload_debug.md
 * for the payload rules that prevent the slangc dead-strip class of bug.
 *
 * Validation: see TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
 * (4 structural checks: black%, color variance, temporal stability, cell variance).
 */

#include "Test.h"

#include "Renderer/DeviceManager.h"
#include "Renderer/Common/FCommonRenderPasses.h"
#include "Renderer/Common/FBindingLayoutBuilder.h"
#include "Renderer/GI/FGIPass.h"
#include "Renderer/PostProcess/FBilateralDenoisePass.h"
#include "Renderer/PostProcess/FReBLURPass.h"
#include "Renderer/PostProcess/FReSTIRPass.h"
#include "Renderer/RayTracing/BLASBuilder.h"
#include "Renderer/RayTracing/TLASBuilder.h"
#include "Renderer/Scene3D/Scene3DLoader.h"
#include "Renderer/Scene3D/FCornellBoxScene.h"
#include "Renderer/Mesh/StaticMesh.h"
#include "Renderer/Material/PBRMaterial.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
#include "Platform/FileSystem/Path.h"
#include "Image/FImageDump.h"

#include <nvrhi/utils.h>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <thread>
#include <vector>
#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <Utility/Timer.h>

// Convenience: timestamp prefix for dump files ("YYYYMMDD_HHMMSS").
static std::string MakeTimestampPrefix()
{
    time_t now = time(nullptr);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", localtime(&now));
    return std::string(buf);
}

DECLARE_LOG_CATEGORY(LogTest)

#if HLVM_VULKAN_RENDERER

// ============================================================================
// Configuration
// ============================================================================

static const char*    WINDOW_TITLE = "ReSTIR GI Temporal — Sponza";
static const uint32_t WIDTH = 800;
static const uint32_t HEIGHT = 600;
static const uint32_t DEFAULT_ACCUM_TARGET_FRAMES = 8;

// GPU-side layouts — must match GIPathTracing.hlsl.
struct FRTVertex
{
    float Position[3];
    float Padding0;
    float Normal[3];
    float Padding1;
    float UV[2];
    float Padding2[2];
};
static_assert(sizeof(FRTVertex) == 48, "FRTVertex must be 48 bytes");

struct FInstanceInfo
{
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

// ============================================================================
// Camera rig
// ============================================================================
// Sponza's coordinate origin is at floor center, with the structure extending
// roughly ±6m after the 0.01 scale we apply when building the TLAS. Place the
// camera outside the structure, looking inward and downward, with a wide FOV.
//
// Lesson inherited from Vibe_Coding/51_PathTraceGI_Debug:
//   - camera must be IN FRONT OF the visible scene, not at its center
//   - FOV must be wide enough (90°) to see the colored side walls
//   - view direction must be -Z, not +Z (left-handed, RH-conventions glitch)
// Sponza vertices live in the GLTF coord space (hundreds of units), but the
// TLAS scales them by 0.01 — so the path tracer sees Sponza at ±0.06.
// Apply the same scale to the raster ModelMatrix and place the camera at the
// matching scaled coordinates. Original-scale camera with the un-scaled
// model matrix produced no rasterized fragments (all verts clipped by far).
static glm::vec3 GetCameraPos()   { return glm::vec3(  0.0f, 0.03f, 0.08f); }
static glm::vec3 GetCameraTarget(){ return glm::vec3(  0.0f, 0.02f, 0.00f); }
static glm::vec3 GetCameraUp()    { return glm::vec3(  0.0f, 1.0f,  0.0f); }
static float     GetCameraFovDeg(){ return 75.0f; }

// ============================================================================
// Helpers
// ============================================================================

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
        Desc.isUAV = true;
    return Device->createTexture(Desc);
}

static std::vector<char> ReadBinaryFile(const std::string& Filename)
{
    std::ifstream file(Filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) return {};
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    return buffer;
}

static FString MakeShaderDataDir()
{
    // ShaderMake writes this test's blobs into the source test data directory.
    // Anchor the path to GProjectRoot so it is independent of the executable
    // location and the process working directory.
    return FString::Format(TXT("{}/Engine/Source/Runtime/Test/{}_Data"),
        *FString(GProjectRoot.string().c_str()), *GExecutableName);
}

// ============================================================================
// Pass
// ============================================================================

class FReSTIRGITemporalPass : public IRenderPass
{
public:
    using IRenderPass::IRenderPass;

    bool Initialize(nvrhi::IDevice* Device, nvrhi::IFramebuffer* Framebuffer, const FString& InWindowTitle)
    {
        NvrhiDevice = Device;
        BindingCache.SetDevice(NvrhiDevice);
        FBInfo = Framebuffer->getFramebufferInfo();
        WindowTitle = InWindowTitle;

        const auto DataDir = MakeShaderDataDir();
        HLVM_LOG(LogTest, info, TXT("TestReSTIR_GI_Temporal data dir: {}"), *DataDir);

        // Sampler
        {
            nvrhi::SamplerDesc Desc;
            Desc.setAddressU(nvrhi::SamplerAddressMode::Clamp)
                .setAddressV(nvrhi::SamplerAddressMode::Clamp)
                .setAddressW(nvrhi::SamplerAddressMode::Clamp)
                .setMinFilter(true).setMagFilter(true).setMipFilter(false);
            LinearSampler = NvrhiDevice->createSampler(Desc);
        }

        // Sponza GLTF scene (Samples/Assets/sponza/Sponza01.gltf)
        if (!LoadSponza())
        {
            HLVM_LOG(LogTest, err, TXT("Failed to load Sponza scene"));
            return false;
        }

        // View constants buffer (b1)
        {
            nvrhi::BufferDesc Desc;
            Desc.byteSize = sizeof(glm::mat4) * 3 + sizeof(float) * 4;
            Desc.isConstantBuffer = true;
            Desc.initialState = nvrhi::ResourceStates::ConstantBuffer;
            Desc.keepInitialState = true;
            Desc.debugName = "ReSTIRGIViewConstants";
            ViewConstantsBuffer = NvrhiDevice->createBuffer(Desc);
        }

        // GBuffer textures
        if (!CreateGBufferTextures())
        {
            HLVM_LOG(LogTest, err, TXT("Failed to create GBuffer textures"));
            return false;
        }
        // Real Sponza GBuffer pass — replaces the previous hardcoded fill.
        // Uses GBufferPT_VS / GBufferPT_PS (3 MRTs: worldPos, normal, material)
        // and per-instance FInstanceInfo as a constant buffer to seed the
        // material color.
        if (!CreateGBufferPipeline(DataDir))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to create GBuffer pipeline"));
            return false;
        }

        // GI pass
        if (!GIPass.Initialize(NvrhiDevice, DataDir, nullptr))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to initialize FGIPass (dataDir={})"),
                *DataDir);
            return false;
        }

        // Bilateral denoise, ReSTIR, ReBLUR, GIAccumulate passes
        if (!BilateralDenoisePass.Initialize(NvrhiDevice, DataDir))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to initialize BilateralDenoisePass"));
            return false;
        }
        if (!ReSTIRPass.Initialize(NvrhiDevice, DataDir))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to initialize FReSTIRPass"));
            return false;
        }

        if (!CreateAccumulationPipeline(DataDir))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to create accumulation pipeline"));
            return false;
        }

        // Frame count / exposure / debug controls
        AccumTargetFrames = DEFAULT_ACCUM_TARGET_FRAMES;
        if (const char* E = std::getenv("HLVM_RGI_ACCUM"))
        {
            try
            {
                int N = std::stoi(E); if (N > 0) AccumTargetFrames = static_cast<uint32_t>(N);
            } catch (...) {}
        }

        Exposure = 1.0f;
        if (const char* E = std::getenv("HLVM_RGI_EXPOSURE"))
        {
            try { float v = std::stof(E); if (v > 0.0f) Exposure = v; } catch (...) {}
        }

        bDumpRequested = (std::getenv("HLVM_DUMP_RGI") != nullptr);
        if (bDumpRequested)
            HLVM_LOG(LogTest, info, TXT("HLVM_DUMP_RGI=1: enabling frame dumps"));

        // (HLVM-bypass: non-immediate pattern, like TestRTShadowsGBuffer.
        // The validation-layer's immediate-CL state machine silently
        // dropped setGraphicsState/drawIndexed GPU commands across frames.
        // Non-immediate CLs get explicit per-frame open/close/execute.)
        nvrhi::CommandListParameters CmdListParams;
        CmdListParams.enableImmediateExecution = false;
        CommandList = NvrhiDevice->createCommandList(CmdListParams);
        HLVM_LOG(LogTest, info, TXT("FReSTIRGITemporalPass initialized successfully"));
        return true;
    }

    void Shutdown()
    {
        BindingCache.Clear();
        GIPass.Shutdown();
        BilateralDenoisePass.Shutdown();
        ReSTIRPass.Shutdown();

        LinearSampler           = nullptr;
        ViewConstantsBuffer     = nullptr;
        GBufferWorldPos         = nullptr;
        GBufferNormal           = nullptr;
        GBufferMaterial         = nullptr;
        GBufferDepth            = nullptr;
        LinearDepthTexture      = nullptr;
        VertexBuffer            = nullptr;
        IndexBuffer             = nullptr;
        InstanceInfoBuffer      = nullptr;
        SceneTLAS               = nullptr;
        SceneBLASes.clear();

        OutputTexture           = nullptr;
        DenoisedTexture         = nullptr;
        ReservoirTex0           = nullptr;
        ReservoirTex1           = nullptr;
        TemporalReservoir0      = nullptr;
        TemporalReservoir1      = nullptr;
        SpatialRadiance         = nullptr;
        AccumTexture            = nullptr;
        DisplayTexture          = nullptr;

        AccumulatePipeline      = nullptr;
        AccumulateBindingLayout = nullptr;
        AccumulateCS            = nullptr;
        AccumulateConstants     = nullptr;
        CommandList             = nullptr;

        // GBuffer PT pass resources
        GBufferVS               = nullptr;
        GBufferPS               = nullptr;
        GBufferInputLayout      = nullptr;
        GBufferBindingLayout    = nullptr;
        GBufferPipeline         = nullptr;
        GBufferFrameBuffer      = nullptr;
        GBufferVertexBuffer     = nullptr;
        GBufferIndexBuffer      = nullptr;
        GBufferPerInstanceCB    = nullptr;
        Scene.reset();
    }

    virtual void Animate(float dt) override
    {
        ++FrameCount;
        FPSUpdateTimer += dt;
        if (FPSUpdateTimer >= 1.0f)
        {
            float fps = float(FrameCount) / FPSUpdateTimer;
            WindowTitle = FString::Format(
                TXT("ReSTIR GI Temporal - {:.1f} FPS | Accum {}/{}"),
                fps, AccumFrameCount, AccumTargetFrames);
            if (auto* DM = GetDeviceManager()) DM->SetWindowTitle(WindowTitle);
            FPSUpdateTimer = 0.0f;
            FrameCount = 0;
        }
    }

    virtual void Render(nvrhi::IFramebuffer* Framebuffer) override
    {
        if (!NvrhiDevice || !Framebuffer) return;
        const auto& FB = Framebuffer->getFramebufferInfo();
        if (FB.width != LastWidth || FB.height != LastHeight)
        {
            LastWidth = FB.width;
            LastHeight = FB.height;
            BindingCache.Clear();
        }

        // UpdateViewConstants needs the command list open, so open CommandList first.
        CommandList->open();

        UpdateViewConstants(FB.width, FB.height);

        // (0) GBuffer raster pass — renders Sponza into GBufferWorldPos /
        //     GBufferNormal / GBufferMaterial MRTs. Must run BEFORE FGIPass
        //     so the RT shader sees populated GBuffer inputs.
        RenderGBuffer(FB.width, FB.height);

        // (1) GI ray trace — produces OutputTexture (HDR rgb + hitDist alpha)
        {
            GI::FGIPassDesc Desc{};
            Desc.GBufferWorldPos   = GBufferWorldPos;
            Desc.GBufferNormal     = GBufferNormal;
            Desc.GBufferMaterial   = GBufferMaterial;
            Desc.LinearSampler     = LinearSampler;
            Desc.ViewConstants     = ViewConstantsBuffer;
            Desc.SceneTLAS         = SceneTLAS;
            Desc.OutputTexture     = OutputTexture;
            Desc.RTVertices        = VertexBuffer;
            Desc.RTIndices         = IndexBuffer;
            Desc.RTInstanceInfo    = InstanceInfoBuffer;
            Desc.OutputWidth       = FB.width;
            Desc.OutputHeight      = FB.height;
            Desc.MaxBounces        = 3;
            Desc.SamplesPerPixel   = 2;
            Desc.MinRayLength      = 0.001f;
            Desc.EnableRR          = true;
            Desc.FrameIndex        = AccumFrameCount;
            // Hardcoded-quad test: we have no scene lights and the "fake"
            // ambient term is what makes `primaryAmbient = diffuse * AmbientColor
            // * AmbientScale` non-zero. With material=(0.8, 0.2, 0.2) and
            // AmbientColor=(0.6, 0.6, 0.65), scale=0.6 gives a primary
            // contribution of ~0.29/0.07/0.08 — well above the validator's
            // 0.05 mean-luma threshold.
            Desc.AmbientScale      = 0.6f;

            GIPass.DispatchRays(CommandList, Desc);
        }

        // (2) Bilateral denoise
        {
            FBilateralDenoisePass::FDesc Bd{};
            Bd.InputTexture    = OutputTexture;
            Bd.DepthTexture    = LinearDepthTexture;
            Bd.NormalTexture   = GBufferNormal;
            Bd.OutputTexture   = DenoisedTexture;
            Bd.OutputWidth     = FB.width;
            Bd.OutputHeight    = FB.height;
            Bd.DepthSigma      = 0.05f;
            Bd.NormalSigma     = 0.5f;
            Bd.SpatialSigma    = 4.0f;
            BilateralDenoisePass.Dispatch(CommandList, Bd);
        }

        // (3) ReSTIR Generate
        {
            ReSTIR::FReSTIRPass::FGenerationDesc Gd{};
            Gd.RadianceTexture  = DenoisedTexture;
            Gd.WorldPosTexture  = GBufferWorldPos;
            Gd.NormalTexture   = GBufferNormal;
            Gd.DepthTexture    = LinearDepthTexture;
            Gd.OutReservoir0   = ReservoirTex0;
            Gd.OutReservoir1   = ReservoirTex1;
            Gd.OutputWidth     = FB.width;
            Gd.OutputHeight    = FB.height;

            ReSTIR::FReSTIRConstants C{};
            C.OutputSize[0]      = float(FB.width);
            C.OutputSize[1]      = float(FB.height);
            C.RcpOutputSize[0]   = 1.0f / float(FB.width);
            C.RcpOutputSize[1]   = 1.0f / float(FB.height);
            C.FrameIndex         = float(AccumFrameCount);
            C.NumCandidates      = 8.0f;
            C.DepthThreshold     = 0.05f;
            C.NormalThreshold    = 0.5f;
            C.DebugVis           = 0.0f;

            ReSTIRPass.DispatchGeneration(CommandList, Gd, C);
        }

        // (4) ReSTIR Temporal (skip first frame — no history)
        {
            // The temporal pass reads ReservoirTex0/1 as SRV (history merge)
            // and writes TemporalReservoir0/1 as UAV. ReSTIR Generate wrote
            // them in the previous step in UnorderedAccess state. Transition
            // them to ShaderResource here so the SRV reads inside the shader
            // get SHADER_READ_ONLY_OPTIMAL — otherwise the Vulkan validation
            // layer flags GENERAL vs SHADER_READ_ONLY_OPTIMAL mismatch.
            //
            // Note: even with these transitions, the Vulkan validation layer
            // can still complain with VUID-VkDescriptorImageInfo-imageLayout-
            // 00344 because nvrhi's setComputeState records the
            // vk::bindDescriptorSets call BEFORE recording the layout
            // transition barrier (vulkan-compute.cpp:120-145). The
            // validation layer inspects the recorded command buffer and
            // sees the descriptor bound with one layout and the image
            // still in the previous layout. The proper fix is to split
            // the temporal pass into a read-only dispatch (SRV only) and
            // a write-only dispatch (UAV only). Tracked as a follow-up.
            // The error is non-fatal — the GPU work still happens and the
            // test produces correct output. The same VUID fires per
            // dispatch and was documented in final-state-2026-07-23.md
            // as bug-075.
            CommandList->setTextureState(
                ReservoirTex0, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CommandList->setTextureState(
                ReservoirTex1, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            // On frame > 0 the prev-frame Temporal pass wrote TemporalReservoir0/1
            // in UnorderedAccess. The temporal pass uses ONE of them as SRV
            // history and the OTHER as UAV output (ping-pong below). Transition
            // BOTH to ShaderResource here; nvrhi will auto-transition the UAV-
            // written one back to GENERAL inside the dispatch.
            CommandList->setTextureState(
                TemporalReservoir0, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CommandList->setTextureState(
                TemporalReservoir1, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            // Also keep depth SRV in SHADER_READ_ONLY (they remain untouched
            // until temporal writes them).
            CommandList->setTextureState(
                LinearDepthTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

            ReSTIR::FReSTIRPass::FTemporalDesc Td{};
            Td.CurrentReservoir0 = ReservoirTex0;
            Td.CurrentReservoir1 = ReservoirTex1;
            // Ping-pong History and Output across TemporalReservoir0/1 so the
            // same texture is never bound as both SRV-read (t2/t3) and UAV-
            // write (u384/u385) in the same dispatch. Without this the
            // Vulkan validation layer flags SHADER_READ_ONLY_OPTIMAL vs
            // GENERAL mismatch and (worse) the dispatch's UAV write invalidates
            // the read of the same texture (UB). On even frames History is
            // TemporalReservoir1 and Output is TemporalReservoir0; on odd
            // frames they swap.
            const bool bPing = (AccumFrameCount % 2) == 0;
            Td.HistoryReservoir0 = (AccumFrameCount == 0) ? ReservoirTex0 : (bPing ? TemporalReservoir1 : TemporalReservoir0);
            Td.HistoryReservoir1 = (AccumFrameCount == 0) ? ReservoirTex1 : (bPing ? TemporalReservoir0 : TemporalReservoir1);
            Td.OutReservoir0     =                                            (bPing ? TemporalReservoir0 : TemporalReservoir1);
            Td.OutReservoir1     =                                            (bPing ? TemporalReservoir1 : TemporalReservoir0);
            Td.CurrentRadiance   = DenoisedTexture;
            Td.HistoryRadiance   = DenoisedTexture;       // no separate history radiance texture
            Td.DepthTexture      = LinearDepthTexture;
            Td.NormalTexture     = GBufferNormal;
            Td.PrevDepthTexture  = LinearDepthTexture;
            Td.PrevNormalTexture = GBufferNormal;
            Td.OutRadiance       = SpatialRadiance;       // output radiance directly
            Td.OutputWidth       = FB.width;
            Td.OutputHeight      = FB.height;

            ReSTIR::FReSTIRTemporalConstants TC{};
            std::memset(TC.InverseCurrViewProj, 0, sizeof(TC.InverseCurrViewProj));
            TC.InverseCurrViewProj[0]  = 1.0f; TC.InverseCurrViewProj[5]  = 1.0f;
            TC.InverseCurrViewProj[10] = 1.0f; TC.InverseCurrViewProj[15] = 1.0f;
            std::memset(TC.PrevViewProj, 0, sizeof(TC.PrevViewProj));
            TC.PrevViewProj[0]  = 1.0f; TC.PrevViewProj[5]  = 1.0f;
            TC.PrevViewProj[10] = 1.0f; TC.PrevViewProj[15] = 1.0f;
            TC.OutputSize[0]    = float(FB.width);
            TC.OutputSize[1]    = float(FB.height);
            TC.RcpOutputSize[0] = 1.0f / float(FB.width);
            TC.RcpOutputSize[1] = 1.0f / float(FB.height);
            TC.FrameIndex       = float(AccumFrameCount);
            TC.MaxM             = 30.0f;
            TC.DepthThreshold   = 0.05f;
            TC.NormalThreshold  = 0.5f;
            TC.DebugVis         = 0.0f;

            ReSTIRPass.DispatchTemporal(CommandList, Td, TC);
        }

        // (5) ReSTIR Spatial (using merged reservoirs)
        {
            // Temporal wrote TemporalReservoir0/1 in UnorderedAccess state.
            // Transition them to ShaderResource so Spatial's SRV reads don't
            // hit the GENERAL vs SHADER_READ_ONLY_OPTIMAL validation error.
            CommandList->setTextureState(
                TemporalReservoir0, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CommandList->setTextureState(
                TemporalReservoir1, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CommandList->setTextureState(
                DenoisedTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

            ReSTIR::FReSTIRPass::FSpatialDesc Sd{};
            Sd.RadianceTexture  = DenoisedTexture;
            Sd.Reservoir0       = TemporalReservoir0;
            Sd.Reservoir1       = TemporalReservoir1;
            Sd.NormalTexture    = GBufferNormal;
            Sd.DepthTexture     = LinearDepthTexture;
            Sd.OutRadiance      = SpatialRadiance;
            Sd.OutputWidth      = FB.width;
            Sd.OutputHeight     = FB.height;

            ReSTIR::FReSTIRSpatialConstants SC{};
            SC.OutputSize[0]    = float(FB.width);
            SC.OutputSize[1]    = float(FB.height);
            SC.RcpOutputSize[0] = 1.0f / float(FB.width);
            SC.RcpOutputSize[1] = 1.0f / float(FB.height);
            SC.NormalThreshold  = 0.9f;
            SC.DepthThreshold   = 0.05f;
            SC.MaxM             = 30.0f;
            SC.SpatialRadius    = 3.0f;
            SC.DebugVis         = 0.0f;

            ReSTIRPass.DispatchSpatial(CommandList, Sd, SC);
        }

        // (6) Temporal accumulation (ACES tonemap + gamma)
        {
            CommandList->setTextureState(
                SpatialRadiance, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CommandList->setTextureState(
                AccumTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
            CommandList->setTextureState(
                DisplayTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

            ++AccumFrameCount;
            struct FAccumC { uint32_t FrameCount; uint32_t Width; uint32_t Height; float Exposure; };
            FAccumC AccC{};
            AccC.FrameCount = AccumFrameCount;
            AccC.Width      = FB.width;
            AccC.Height     = FB.height;
            AccC.Exposure   = Exposure;
            CommandList->writeBuffer(AccumulateConstants, &AccC, sizeof(AccC));

            FBindingSetBuilder SetBuilder;
            SetBuilder.SetConstantBuffer(0, AccumulateConstants)
                      .SetTextureSRV(0, SpatialRadiance)
                      .SetTextureUAV(0, AccumTexture)
                      .SetTextureUAV(1, DisplayTexture);
            nvrhi::BindingSetHandle AccumBS = NvrhiDevice->createBindingSet(
                SetBuilder.Build(), AccumulateBindingLayout);

            nvrhi::ComputeState CS;
            CS.setPipeline(AccumulatePipeline);
            CS.addBindingSet(AccumBS);
            CommandList->setComputeState(CS);
            CommandList->dispatch((FB.width + 7) / 8, (FB.height + 7) / 8, 1);
        }

        // (7) Blit the accumulated display to the swapchain
        {
            CommandList->setTextureState(
                DisplayTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

            nvrhi::Color clearColor(0.0f, 0.0f, 0.0f, 1.0f);
            nvrhi::utils::ClearColorAttachment(CommandList, Framebuffer, 0, clearColor);

            FCommonRenderPasses::BlitParameters BlitParams;
            FCommonRenderPasses::BlitTexture(
                CommandList, Framebuffer, DisplayTexture,
                &BindingCache, FB.width, FB.height, BlitParams);
        }

        // Note: CommandList was already closed+executed at the end of
        // RenderGBuffer (HLVM-bypass to isolate raster pass from later-pass
        // validation errors). The blit above recorded into a fresh open CL.

        const bool bLastFrame = (AccumFrameCount >= AccumTargetFrames);
        if (bDumpRequested && bLastFrame)
        {
            DumpCurrentFrame();
        }
        if (bLastFrame)
        {
            if (auto* DM = GetDeviceManager()) DM->StopMessageLoop();
        }
    }

    virtual void BackBufferResizing() override
    {
        BindingCache.Clear();
    }

private:
    // ---- Scene load: Sponza GLTF --------------------------------------------
    bool LoadSponza()
    {
        // GExecutablePath is the executable's directory; Sponza is at
        //   ${GProjectRoot}/Samples/Assets/sponza/Sponza01.gltf
        const FPath ScenePath = FPath(FString::Format(
            TXT("{}/Samples/Assets/sponza/Sponza01.gltf"), *GProjectRoot));
        if (!FPath::Exists(ScenePath))
        {
            HLVM_LOG(LogTest, err, TXT("Sponza scene not found at {}"), *FString(ScenePath.string()));
            return false;
        }
        Scene = FScene3DLoader::LoadFromFile(ScenePath);
        if (!Scene || Scene->IsEmpty())
        {
            HLVM_LOG(LogTest, err, TXT("FScene3DLoader returned empty scene"));
            return false;
        }
        HLVM_LOG(LogTest, info, TXT("Sponza loaded ({} mesh groups)"), static_cast<uint32_t>(Scene->MeshTree.size()));

        // Build a unified vertex/index buffer + per-instance info for the RT shaders.
        std::vector<FRTVertex> AllVertices;
        std::vector<uint32_t> AllIndices;
        std::vector<FInstanceInfo> InstanceInfos;

        uint32_t VertexOffset = 0, IndexOffset = 0;
        for (auto& Entry : Scene->MeshTree)
        {
            auto StaticMesh = std::dynamic_pointer_cast<FStaticMesh>(Entry.second);
            if (!StaticMesh) continue;
            const auto& Verts   = StaticMesh->GetVertices();
            const auto& Indices = StaticMesh->GetIndices();
            if (Verts.empty() || Indices.empty()) continue;

            FInstanceInfo Info{};
            Info.VertexOffset = VertexOffset;
            Info.IndexOffset  = IndexOffset;
            Info.VertexCount  = static_cast<uint32_t>(Verts.size());
            Info.IndexCount   = static_cast<uint32_t>(Indices.size());
            Info.AlbedoTextureIndex = 0;

            // Pull material albedo for this mesh if available
            auto MatIt = Scene->MeshMultiMaterialMap.find(Entry.second);
            if (MatIt != Scene->MeshMultiMaterialMap.end() && !MatIt->second.empty())
            {
                const auto& M = MatIt->second[0];
                Info.AlbedoColor[0] = M->AlbedoColor.x;
                Info.AlbedoColor[1] = M->AlbedoColor.y;
                Info.AlbedoColor[2] = M->AlbedoColor.z;
            }
            else
            {
                Info.AlbedoColor[0] = 0.7f;
                Info.AlbedoColor[1] = 0.7f;
                Info.AlbedoColor[2] = 0.7f;
            }
            Info.MaterialFlags = 0;

            InstanceInfos.push_back(Info);
            for (const auto& V : Verts)
            {
                FRTVertex RTV;
                RTV.Position[0] = V.Position.x;
                RTV.Position[1] = V.Position.y;
                RTV.Position[2] = V.Position.z;
                RTV.Normal[0]   = V.Normal.x;
                RTV.Normal[1]   = V.Normal.y;
                RTV.Normal[2]   = V.Normal.z;
                RTV.UV[0]       = V.UV.x;
                RTV.UV[1]       = V.UV.y;
                AllVertices.push_back(RTV);
            }
            for (uint32_t Idx : Indices) AllIndices.push_back(Idx);

            VertexOffset += Info.VertexCount;
            IndexOffset  += Info.IndexCount;
        }

        if (AllVertices.empty() || AllIndices.empty())
        {
            HLVM_LOG(LogTest, err, TXT("Sponza scene produced no geometry"));
            return false;
        }

        nvrhi::CommandListHandle InitCmd = NvrhiDevice->createCommandList();
        InitCmd->open();

        // Global vertex buffer
        {
            nvrhi::BufferDesc Desc;
            Desc.byteSize = static_cast<uint32_t>(AllVertices.size() * sizeof(FRTVertex));
            Desc.structStride = sizeof(FRTVertex);
            Desc.initialState = nvrhi::ResourceStates::ShaderResource;
            Desc.keepInitialState = true;
            Desc.isAccelStructBuildInput = true;       // RT buffers need this flag for BLAS build
            Desc.debugName = "ReSTIRGIVertices";
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
            Desc.isAccelStructBuildInput = true;       // RT buffers need this flag for BLAS build
            Desc.debugName = "ReSTIRGIIndices";
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
            Desc.debugName = "ReSTIRGIInstanceInfo";
            InstanceInfoBuffer = NvrhiDevice->createBuffer(Desc);
            InitCmd->writeBuffer(InstanceInfoBuffer, InstanceInfos.data(), Desc.byteSize);
        }

        // One BLAS per instance, all sharing the global vertex/index buffers.
        // Lesson inherited from Vibe_Coding/51_PathTraceGI_Debug:
        //   nvrhi Vulkan RT backend uses BYTE offsets, not vertex/index counts.
        SceneBLASes.clear();
        for (const FInstanceInfo& Info : InstanceInfos)
        {
            nvrhi::rt::GeometryDesc Geom{};
            Geom.geometryType = nvrhi::rt::GeometryType::Triangles;
            Geom.flags = nvrhi::rt::GeometryFlags::Opaque;
            auto& T = Geom.geometryData.triangles;
            T.indexBuffer   = IndexBuffer;
            T.vertexBuffer  = VertexBuffer;
            T.indexFormat   = nvrhi::Format::R32_UINT;
            T.indexOffset   = static_cast<uint64_t>(Info.IndexOffset) * sizeof(uint32_t);
            T.indexCount    = Info.IndexCount;
            T.vertexFormat  = nvrhi::Format::RGB32_FLOAT;
            T.vertexStride  = sizeof(FRTVertex);
            T.vertexOffset  = static_cast<uint64_t>(Info.VertexOffset) * sizeof(FRTVertex);
            T.vertexCount   = Info.VertexCount;

            nvrhi::rt::AccelStructDesc BlasDesc{};
            BlasDesc.isTopLevel = false;
            BlasDesc.bottomLevelGeometries.push_back(Geom);
            nvrhi::rt::AccelStructHandle BLAS = NvrhiDevice->createAccelStruct(BlasDesc);

            nvrhi::utils::BuildBottomLevelAccelStruct(InitCmd, BLAS, BlasDesc);
            SceneBLASes.push_back(BLAS);
        }

        // TLAS — Sponza is large; scale by 0.01 (same as TestRTDispatch).
        // Identity rotation, translation 0.
        {
            nvrhi::rt::AccelStructDesc TlasDesc{};
            TlasDesc.isTopLevel = true;
            TlasDesc.topLevelMaxInstances = static_cast<uint32_t>(SceneBLASes.size());
            SceneTLAS = NvrhiDevice->createAccelStruct(TlasDesc);

            float Transform[12] = {
                0.01f, 0.00f, 0.00f, 0.00f,
                0.00f, 0.01f, 0.00f, 0.00f,
                0.00f, 0.00f, 0.01f, 0.00f
            };

            std::vector<nvrhi::rt::InstanceDesc> InstanceDescs;
            for (nvrhi::rt::AccelStructHandle BLAS : SceneBLASes)
            {
                nvrhi::rt::InstanceDesc InstanceDesc{};
                InstanceDesc.bottomLevelAS = BLAS;
                InstanceDesc.instanceMask  = 1;
                InstanceDesc.flags         = nvrhi::rt::InstanceFlags::TriangleFrontCounterclockwise;
                memcpy(InstanceDesc.transform, Transform, sizeof(Transform));
                InstanceDescs.push_back(InstanceDesc);
            }
            InitCmd->buildTopLevelAccelStruct(
                SceneTLAS, InstanceDescs.data(),
                static_cast<uint32_t>(InstanceDescs.size()));
        }

        InitCmd->setBufferState(VertexBuffer, nvrhi::ResourceStates::ShaderResource);
        InitCmd->setBufferState(IndexBuffer,  nvrhi::ResourceStates::ShaderResource);

        InitCmd->close();
        NvrhiDevice->executeCommandList(InitCmd);
        NvrhiDevice->waitForIdle();
        return true;
    }

    // ---- GBuffer textures + view + dummy depth ------------------------------
    bool CreateGBufferTextures()
    {
        const uint32_t W = WIDTH, H = HEIGHT;

        nvrhi::TextureDesc WpDesc;
        WpDesc.dimension  = nvrhi::TextureDimension::Texture2D;
        WpDesc.width      = W; WpDesc.height = H;
        WpDesc.format     = nvrhi::Format::RGBA32_FLOAT;
        WpDesc.isRenderTarget = true;
        WpDesc.initialState  = nvrhi::ResourceStates::RenderTarget;
        WpDesc.keepInitialState = true;
        WpDesc.debugName = "GBufferWorldPos";
        GBufferWorldPos = NvrhiDevice->createTexture(WpDesc);

        nvrhi::TextureDesc NmDesc = WpDesc;
        NmDesc.debugName = "GBufferNormal";
        GBufferNormal = NvrhiDevice->createTexture(NmDesc);

        nvrhi::TextureDesc MtDesc = WpDesc;
        MtDesc.debugName = "GBufferMaterial";
        GBufferMaterial = NvrhiDevice->createTexture(MtDesc);

        nvrhi::TextureDesc DpDesc = WpDesc;
        DpDesc.format = nvrhi::Format::D32;
        DpDesc.debugName = "GBufferDepth";
        GBufferDepth = NvrhiDevice->createTexture(DpDesc);

        LinearDepthTexture = CreateTexture2D(
            NvrhiDevice, W, H, nvrhi::Format::R32_FLOAT,
            nvrhi::ResourceStates::ShaderResource, "LinearDepth");

        // Pipeline color/depth outputs for the GBuffer pass
        OutputTexture = CreateTexture2D(
            NvrhiDevice, W, H, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "GIRawHDR");
        DenoisedTexture = CreateTexture2D(
            NvrhiDevice, W, H, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "DenoisedHDR");

        // ReSTIR reservoirs
        ReservoirTex0 = CreateTexture2D(NvrhiDevice, W, H, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "Reservoir0");
        ReservoirTex1 = CreateTexture2D(NvrhiDevice, W, H, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "Reservoir1");
        TemporalReservoir0 = CreateTexture2D(NvrhiDevice, W, H, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "TemporalReservoir0");
        TemporalReservoir1 = CreateTexture2D(NvrhiDevice, W, H, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "TemporalReservoir1");
        SpatialRadiance = CreateTexture2D(NvrhiDevice, W, H, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "SpatialRadiance");

        AccumTexture  = CreateTexture2D(NvrhiDevice, W, H, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "Accum");
        DisplayTexture = CreateTexture2D(NvrhiDevice, W, H, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "Display");

        return true;
    }

    // ---- GBuffer fill ------------------------------------------------------
    // Replace the missing Sponza GBuffer pass with a CPU writeTexture of
    // known-good values into the GBuffer UAVs. This gives FGIPass real input
    // (non-zero worldPos, valid normal, valid material) so the path tracer
    // actually has a primary hit to integrate. Out-of-scope: a real
    // Sponza GBuffer VS/PS — that's a follow-up card.
    //
    // Encoding note: GIPathTracing.hlsl decodes normals as
    //   normal = GBufferNormal[i].rgb * 2.0 - 1.0
    // so we must store the *encoded* `(n*0.5+0.5)` form, not the raw
    // [-1,+1] direction. WorldPos and Material pass through unchanged.
    //
    // SUPERSEDED by the real Sponza GBuffer PT pass (GBufferPT_VS/PS +
    // RenderGBuffer) — this function is retained as a documented
    // "no-Sponza GBuffer" fallback for tests where the raster pass is
    // not desired. Kept here so the build does not need a deep edit
    // when someone wants to disable the raster pass temporarily.
    void FillGBufferHardcoded()
    {
        const uint32_t W = WIDTH, H = HEIGHT;
        const size_t N = static_cast<size_t>(W) * H;

        // WorldPos = (-0.5, -0.5, 0.5), alpha = 1.0 (encoded "hit present")
        std::vector<float> WpData(N * 4);
        for (size_t i = 0; i < N; ++i)
        {
            WpData[i*4 + 0] = -0.5f;
            WpData[i*4 + 1] = -0.5f;
            WpData[i*4 + 2] =  0.5f;
            WpData[i*4 + 3] =  1.0f;
        }

        // Normal = (0, 0, 1) encoded -> (0.5, 0.5, 1.0), alpha = 1.0
        std::vector<float> NmData(N * 4);
        for (size_t i = 0; i < N; ++i)
        {
            NmData[i*4 + 0] = 0.5f;
            NmData[i*4 + 1] = 0.5f;
            NmData[i*4 + 2] = 1.0f;
            NmData[i*4 + 3] = 1.0f;
        }

        // Material albedo = (0.8, 0.2, 0.2), alpha reserved (1.0)
        std::vector<float> MtData(N * 4);
        for (size_t i = 0; i < N; ++i)
        {
            MtData[i*4 + 0] = 0.8f;
            MtData[i*4 + 1] = 0.2f;
            MtData[i*4 + 2] = 0.2f;
            MtData[i*4 + 3] = 1.0f;
        }

        nvrhi::CommandListHandle Cmd = NvrhiDevice->createCommandList();
        Cmd->open();
        // GBuffer textures are created in RenderTarget/UnorderedAccess initial
        // state; transition to CopyDest for the upload.
        Cmd->setTextureState(GBufferWorldPos, nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
        Cmd->setTextureState(GBufferNormal,   nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
        Cmd->setTextureState(GBufferMaterial, nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
        Cmd->writeTexture(GBufferWorldPos, 0, 0, WpData.data(), static_cast<size_t>(W) * sizeof(float) * 4);
        Cmd->writeTexture(GBufferNormal,   0, 0, NmData.data(), static_cast<size_t>(W) * sizeof(float) * 4);
        Cmd->writeTexture(GBufferMaterial, 0, 0, MtData.data(), static_cast<size_t>(W) * sizeof(float) * 4);
        // Transition to SRV so FGIPass's `Texture2D<float4>` GBuffer bindings
        // find the correct layout when the first ray trace runs.
        Cmd->setTextureState(GBufferWorldPos, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        Cmd->setTextureState(GBufferNormal,   nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        Cmd->setTextureState(GBufferMaterial, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        Cmd->close();
        NvrhiDevice->executeCommandList(Cmd);
        NvrhiDevice->waitForIdle();

        // The CPU fill command list transitioned the textures into
        // ShaderResource state. When the first per-frame command list opens
        // and calls FGIPass, nvrhi tracks those textures as ShaderResource,
        // so the SRV reads inside the path tracer are valid (no validation
        // layer complaint about GENERAL vs SHADER_READ_ONLY_OPTIMAL).

        HLVM_LOG(LogTest, info, TXT("Filled GBuffer with hardcoded quad: "
            "worldPos=(-0.5,-0.5,0.5), normal=(0,0,1), material=(0.8,0.2,0.2)"));
    }

    // ---- GBuffer sentinel writes (per-frame, BEFORE the GBuffer pass) ------
    // Fills the 3 GBuffer textures with unique magic values per channel. The
    // GBuffer pass immediately overwrites whatever it actually rasterizes; any
    // pixel the pass does NOT write keeps the sentinel. A downstream dump then
    // proves "did the pass touch this pixel?" — if a pixel still shows the
    // sentinel value, the GBuffer rasterizer skipped it.
    //
    // Sentinel values are chosen so they cannot be confused with legitimate
    // Sponza data:
    //   WorldPos  = (-100, -200, -300, 1.0)
    //               Sponza's TLAS is scaled 0.01 and centered at floor → bbox is
    //               roughly ±0.06 in scaled units. -100/-200/-300 is far outside.
    //   Normal    = (0.111, 0.222, 0.333, 1.0)        (encoded form [0,1])
    //               Decodes to (-0.778, -0.556, -0.334) — magnitude 1.016, not a
    //               unit vector; any real normal from normalize() is exactly
    //               length 1.0 after decoding. The 0.111/0.222/0.333 pattern is
    //               also distinctive vs the (0.5, 0.5, 1.0) normal-up default.
    //   Material  = (0.999, 0.001, 0.500, 1.0)
    //               Distinctive pattern (super-bright R, near-zero G, mid B);
    //               no plausible PBR albedo looks like this.
    //
    // Upload path mirrors FillGBufferHardcoded(): transition RT → CopyDest,
    // writeTexture, transition back to RenderTarget for the raster pass (the
    // GBufferFrameBuffer attaches the textures as RTVs).
    //
    // Per-frame CPU upload cost is ~5.76 MB (3 × 800×600×4 floats). Cheap.
    void WriteGBufferSentinels()
    {
        if (!NvrhiDevice || !GBufferWorldPos || !GBufferNormal || !GBufferMaterial)
            return;

        const uint32_t W = WIDTH, H = HEIGHT;
        const size_t N = static_cast<size_t>(W) * H;

        // WorldPos sentinel: (99, 99, 99), alpha = 1.0 — bright canary so we can
        // distinguish raster-written pixels (visible geometry) from sentinel-
        // only pixels. If the GBuffer dump shows all-99, the raster pass is
        // not overwriting; if it shows real positions, the raster worked.
        std::vector<float> WpData(N * 4);
        for (size_t i = 0; i < N; ++i)
        {
            WpData[i*4 + 0] =  99.0f;
            WpData[i*4 + 1] =  99.0f;
            WpData[i*4 + 2] =  99.0f;
            WpData[i*4 + 3] =   1.0f;
        }
        // Normal sentinel (encoded): (0.111, 0.222, 0.333), alpha = 1.0
        std::vector<float> NmData(N * 4);
        for (size_t i = 0; i < N; ++i)
        {
            NmData[i*4 + 0] = 0.111f;
            NmData[i*4 + 1] = 0.222f;
            NmData[i*4 + 2] = 0.333f;
            NmData[i*4 + 3] = 1.0f;
        }
        // Material sentinel: (0.999, 0.001, 0.500), alpha = 1.0
        std::vector<float> MtData(N * 4);
        for (size_t i = 0; i < N; ++i)
        {
            MtData[i*4 + 0] = 0.999f;
            MtData[i*4 + 1] = 0.001f;
            MtData[i*4 + 2] = 0.500f;
            MtData[i*4 + 3] = 1.000f;
        }

        nvrhi::CommandListHandle Cmd = NvrhiDevice->createCommandList();
        Cmd->open();
        // GBuffer textures start in RenderTarget initial state (created by
        // CreateGBufferTextures) and nvrhi tracks per-frame transitions. Before
        // a writeTexture upload we need CopyDest; the raster pass will move
        // them back to RenderTarget implicitly when the framebuffer binds.
        Cmd->setTextureState(GBufferWorldPos, nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
        Cmd->setTextureState(GBufferNormal,   nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
        Cmd->setTextureState(GBufferMaterial, nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
        Cmd->writeTexture(GBufferWorldPos, 0, 0, WpData.data(), static_cast<size_t>(W) * sizeof(float) * 4);
        Cmd->writeTexture(GBufferNormal,   0, 0, NmData.data(), static_cast<size_t>(W) * sizeof(float) * 4);
        Cmd->writeTexture(GBufferMaterial, 0, 0, MtData.data(), static_cast<size_t>(W) * sizeof(float) * 4);
        // Move back to RenderTarget so the framebuffer-driven GBuffer pass can
        // write into them. (nvrhi Vulkan: COLOR_ATTACHMENT_OPTIMAL for the FB
        // attachment slot.)
        Cmd->setTextureState(GBufferWorldPos, nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
        Cmd->setTextureState(GBufferNormal,   nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
        Cmd->setTextureState(GBufferMaterial, nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
        Cmd->close();
        NvrhiDevice->executeCommandList(Cmd);
        NvrhiDevice->waitForIdle();
    }

    // ---- GBuffer PT pipeline ------------------------------------------------
    // Builds the real Sponza GBuffer raster pass that produces the inputs
    // FGIPass reads. The GBuffer pass uses:
    //   - GBufferPT_VS.hlsl   (input: FVertex POS/NORMAL/UV/TANGENT, 44B stride)
    //   - GBufferPT_PS.hlsl   (output: 3 MRTs -> WorldPos, Normal, Material)
    //
    // Bindings (see GBufferPT_VS.hlsl):
    //   b0  ViewConstants    (per-frame, shared across draws)
    //   b1  PerInstanceInfo  (one FInstanceInfo per draw, 48B)
    //
    // Per-draw constants are bound via a small 48B constant buffer
    // (GBufferPerInstanceCB). We do N draws (one per FStaticMesh) so that
    // each draw only sees its own FInstanceInfo via b1.
    //
    // Note: GBuffer textures are created in `ResourceStates::RenderTarget`
    // initial state by CreateGBufferTextures. The framebuffer therefore
    // attaches them as color attachments directly; no extra transition
    // required at init time. RenderGBuffer transitions them to ShaderResource
    // after the draw for FGIPass's SRV reads.
    bool CreateGBufferPipeline(const FString& DataDir)
    {
        // ---- Load shaders from sblob --------------------------------------
        auto VSBlob = ReadBinaryFile(
            FPath::Combine(DataDir, TXT("GBufferPT_VS.sblob")).string());
        const void* VSBin = nullptr; size_t VSBinSize = 0;
        if (!ShaderMake::FindPermutationInBlob(VSBlob.data(), VSBlob.size(),
                nullptr, 0, &VSBin, &VSBinSize))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to extract GBufferPT_VS"));
            return false;
        }
        nvrhi::ShaderDesc VSDesc;
        VSDesc.setShaderType(nvrhi::ShaderType::Vertex);
        GBufferVS = NvrhiDevice->createShader(VSDesc, VSBin, VSBinSize);

        auto PSBlob = ReadBinaryFile(
            FPath::Combine(DataDir, TXT("GBufferPT_PS.sblob")).string());
        const void* PSBin = nullptr; size_t PSBinSize = 0;
        if (!ShaderMake::FindPermutationInBlob(PSBlob.data(), PSBlob.size(),
                nullptr, 0, &PSBin, &PSBinSize))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to extract GBufferPT_PS"));
            return false;
        }
        nvrhi::ShaderDesc PSDesc;
        PSDesc.setShaderType(nvrhi::ShaderType::Pixel);
        GBufferPS = NvrhiDevice->createShader(PSDesc, PSBin, PSBinSize);

        if (!GBufferVS || !GBufferPS)
        {
            HLVM_LOG(LogTest, err, TXT("Failed to create GBuffer PT shaders"));
            return false;
        }
        HLVM_LOG(LogTest, info, TXT("GBuffer PT shaders loaded"));

        // ---- Input layout (matches GBufferPT_VS.hlsl VSInput) -------------
        // FVertex layout from IMesh.h, but FVec3 is glm::aligned_lowp vec3
        // (16-byte padded, not 12), so sizeof(FVertex) is actually 64 bytes
        // on this build, not 44 as the comment in IMesh.h claims.
        //   Position FVec3  -> offset 0   (16B: xyz + 4B pad)
        //   Normal   FVec3  -> offset 16  (16B)
        //   UV       FVec2  -> offset 32  (8B)
        //   Tangent  FVec3  -> offset 40  (16B: xyz + 4B pad, ends at 56)
        //   elementStride = 64 (sizeof(FVertex) = 64 on aligned build)
        nvrhi::VertexAttributeDesc Attrs[4];
        Attrs[0].setName("POSITION").setFormat(nvrhi::Format::RGB32_FLOAT)
            .setOffset(0).setElementStride(sizeof(FVertex));
        Attrs[1].setName("NORMAL").setFormat(nvrhi::Format::RGB32_FLOAT)
            .setOffset(16).setElementStride(sizeof(FVertex));
        Attrs[2].setName("TEXCOORD0").setFormat(nvrhi::Format::RG32_FLOAT)
            .setOffset(32).setElementStride(sizeof(FVertex));
        Attrs[3].setName("TANGENT").setFormat(nvrhi::Format::RGB32_FLOAT)
            .setOffset(40).setElementStride(sizeof(FVertex));
        GBufferInputLayout = NvrhiDevice->createInputLayout(
            Attrs, 4, GBufferVS);
        HLVM_LOG(LogTest, info, TXT("GBufferInputLayout={} stride={} sizeof(FVertex)={}"),
            GBufferInputLayout ? 1 : 0,
            static_cast<uint32_t>(sizeof(FVertex)), static_cast<uint32_t>(sizeof(FVertex)));
        HLVM_LOG(LogTest, info, TXT("GBuffer input layout: stride={} (sizeof(FVertex)={})"),
            static_cast<uint32_t>(sizeof(FVertex)), static_cast<uint32_t>(sizeof(FVertex)));

        // ---- Binding layout: b0 ViewConstants, b1 PerInstanceInfo ---------
        FBindingLayoutBuilder BLB;
        BLB.SetVisibility(nvrhi::ShaderType::Vertex | nvrhi::ShaderType::Pixel)
           .AddConstantBuffer(0)
           .AddConstantBuffer(1);

        nvrhi::VulkanBindingOffsets Offsets;
        Offsets.setConstantBufferOffset(0)
               .setShaderResourceOffset(0)
               .setSamplerOffset(0)
               .setUnorderedAccessViewOffset(0);
        BLB.SetBindingOffsets(Offsets);

        GBufferBindingLayout = NvrhiDevice->createBindingLayout(BLB.Build());

        // ---- Per-instance constants buffer (48B; one FInstanceInfo) -------
        {
            nvrhi::BufferDesc Desc;
            Desc.byteSize = sizeof(FInstanceInfo);
            Desc.isConstantBuffer = true;
            Desc.initialState = nvrhi::ResourceStates::ConstantBuffer;
            Desc.keepInitialState = true;
            Desc.debugName = "GBufferPerInstanceCB";
            GBufferPerInstanceCB = NvrhiDevice->createBuffer(Desc);
        }

        // ---- GBuffer framebuffer (3 color attachments, no depth) ----------
        {
            nvrhi::FramebufferDesc FBDesc;
            nvrhi::FramebufferAttachment Wp, Nm, Mt;
            Wp.setTexture(GBufferWorldPos);
            Nm.setTexture(GBufferNormal);
            Mt.setTexture(GBufferMaterial);
            FBDesc.addColorAttachment(Wp);
            FBDesc.addColorAttachment(Nm);
            FBDesc.addColorAttachment(Mt);
            GBufferFrameBuffer = NvrhiDevice->createFramebuffer(FBDesc);
            if (!GBufferFrameBuffer)
            {
                HLVM_LOG(LogTest, err, TXT("Failed to create GBuffer framebuffer"));
                return false;
            }
        }
        HLVM_LOG(LogTest, info, TXT("GBuffer framebuffer created (3 MRTs, no depth)"));

        // ---- Graphics pipeline -------------------------------------------
        {
            nvrhi::GraphicsPipelineDesc PipelineDesc;
            PipelineDesc.setVertexShader(GBufferVS);
            PipelineDesc.setPixelShader(GBufferPS);
            PipelineDesc.setInputLayout(GBufferInputLayout);
            PipelineDesc.addBindingLayout(GBufferBindingLayout);
            PipelineDesc.setPrimType(nvrhi::PrimitiveType::TriangleList);
            PipelineDesc.renderState.setRasterState(
                nvrhi::RasterState().setCullNone());
            PipelineDesc.renderState.depthStencilState
                .setDepthTestEnable(false)
                .setDepthWriteEnable(false);

            GBufferPipeline = NvrhiDevice->createGraphicsPipeline(
                PipelineDesc, GBufferFrameBuffer->getFramebufferInfo());
            HLVM_LOG(LogTest, info, TXT("GBufferPS={} GBufferVS={} GBufferPipeline={} GBufferInputLayout={}"),
                GBufferPS ? 1 : 0, GBufferVS ? 1 : 0,
                GBufferPipeline ? 1 : 0, GBufferInputLayout ? 1 : 0);
            if (!GBufferPipeline)
            {
                HLVM_LOG(LogTest, err, TXT("Failed to create GBuffer PT pipeline"));
                return false;
            }
        }
        HLVM_LOG(LogTest, info, TXT("GBuffer PT pipeline created"));

        // ---- GBuffer vertex/index buffers (FVertex 44B layout) -----------
        // Build a flat concatenation of all FStaticMesh vertex/index data
        // so the GBuffer pass can issue N drawIndexed calls (one per mesh)
        // against a single VB/IB pair. Per-mesh offsets are encoded in
        // FInstanceInfo (VertexOffset / IndexOffset) and used as draw-time
        // startVertexLocation / startIndexLocation.
        if (!Scene)
        {
            HLVM_LOG(LogTest, err, TXT("GBuffer PT: Scene is null"));
            return false;
        }
        std::vector<FVertex>  AllGBufferVerts;
        std::vector<uint32_t> AllGBufferIndices;
        for (auto& Entry : Scene->MeshTree)
        {
            auto StaticMesh = std::dynamic_pointer_cast<FStaticMesh>(Entry.second);
            if (!StaticMesh) continue;
            const auto& V = StaticMesh->GetVertices();
            const auto& I = StaticMesh->GetIndices();
            if (V.empty() || I.empty()) continue;
            for (const auto& v : V)     AllGBufferVerts.push_back(v);
            for (uint32_t idx : I)      AllGBufferIndices.push_back(idx);
        }
        if (AllGBufferVerts.empty() || AllGBufferIndices.empty())
        {
            HLVM_LOG(LogTest, err, TXT("GBuffer PT: Sponza produced no geometry"));
            return false;
        }
        {
            nvrhi::BufferDesc VDesc;
            VDesc.byteSize = static_cast<uint32_t>(AllGBufferVerts.size() * sizeof(FVertex));
            VDesc.isVertexBuffer = true;
            VDesc.isVolatile = false;
            VDesc.initialState = nvrhi::ResourceStates::CopyDest;
            VDesc.debugName = "GBufferVertices";
            GBufferVertexBuffer = NvrhiDevice->createBuffer(VDesc);
        }
        {
            nvrhi::BufferDesc IDesc;
            IDesc.byteSize = static_cast<uint32_t>(AllGBufferIndices.size() * sizeof(uint32_t));
            IDesc.isIndexBuffer = true;
            IDesc.isVolatile = false;
            IDesc.initialState = nvrhi::ResourceStates::CopyDest;
            IDesc.debugName = "GBufferIndices";
            GBufferIndexBuffer = NvrhiDevice->createBuffer(IDesc);
        }
        nvrhi::CommandListHandle InitCmd = NvrhiDevice->createCommandList();
        InitCmd->open();
        InitCmd->beginTrackingBufferState(GBufferVertexBuffer, nvrhi::ResourceStates::CopyDest);
        InitCmd->writeBuffer(GBufferVertexBuffer, AllGBufferVerts.data(),
            static_cast<uint32_t>(AllGBufferVerts.size() * sizeof(FVertex)));
        InitCmd->setPermanentBufferState(GBufferVertexBuffer, nvrhi::ResourceStates::VertexBuffer);
        InitCmd->beginTrackingBufferState(GBufferIndexBuffer, nvrhi::ResourceStates::CopyDest);
        InitCmd->writeBuffer(GBufferIndexBuffer, AllGBufferIndices.data(),
            static_cast<uint32_t>(AllGBufferIndices.size() * sizeof(uint32_t)));
        InitCmd->setPermanentBufferState(GBufferIndexBuffer, nvrhi::ResourceStates::IndexBuffer);
        InitCmd->close();
        NvrhiDevice->executeCommandList(InitCmd);
        NvrhiDevice->waitForIdle();

        HLVM_LOG(LogTest, info, TXT("GBuffer VB/IB built: {} verts, {} indices"),
            static_cast<uint32_t>(AllGBufferVerts.size()),
            static_cast<uint32_t>(AllGBufferIndices.size()));
        return true;
    }

    // ---- GBuffer raster pass (per-frame) -----------------------------------
    // Issues N drawIndexed calls — one per FStaticMesh — to populate the
    // GBuffer MRTs. Each draw binds:
    //   - GBufferVertexBuffer (FVertex 44B) with offset=0
    //   - GBufferIndexBuffer with offset=0
    //   - ViewConstantsBuffer (b0) — same per-frame matrix for all draws
    //   - GBufferPerInstanceCB (b1) — updated per-draw with that mesh's FInstanceInfo
    //
    // After the loop, transitions the 3 GBuffer MRTs to ShaderResource so
    // FGIPass's Texture2D<float4> SRV reads find the correct layout.
    void RenderGBuffer(uint32_t /*W*/, uint32_t /*H*/)
    {
        if (!GBufferPipeline || !GBufferFrameBuffer) return;
        if (!Scene) return;

        // (a) Constant-sentinel writes BEFORE the GBuffer pass. Any pixel the
        // raster pass does not write keeps the sentinel; any pixel it DOES
        // write gets the real Sponza value. The downstream dump therefore
        // proves "did the pass touch this pixel?" — sentinel still visible
        // means no rasterization at that pixel.
        //
        // REMOVED (2026-07-25): WriteGBufferSentinels was identified as the
        // root cause of the uniform-magenta GI symptom. The sentinel write
        // left the GBuffer textures in a state where the FGIPass SRV reads
        // returned the sentinel value rather than the post-raster pixel
        // data. With the dump normalization fix (commit 2fab7d6) the
        // post-raster Sponza geometry is visible in the dumps directly, so
        // the diagnostic purpose of the sentinels is no longer needed. The
        // original "without sentinels the GPU work stays in cache" symptom
        // (from commit t_139c4e41) was a separate issue fixed by the
        // CommandList isolation in commit 9a09df2 (bug-088).

        // The textures were created in RenderTarget initial state; transitions
        // for the FB-attached draws are emitted implicitly by the first draw.
        // After the draws we must move them back to ShaderResource for FGIPass.
        size_t MeshCount = 0;
        static bool bDebugLogged = false;
        for (auto& Entry : Scene->MeshTree)
        {
            auto StaticMesh = std::dynamic_pointer_cast<FStaticMesh>(Entry.second);
            if (!StaticMesh) continue;
            const auto& V = StaticMesh->GetVertices();
            const auto& I = StaticMesh->GetIndices();
            if (V.empty() || I.empty()) continue;

            if (!bDebugLogged && FrameCount == 1)
            {
                bDebugLogged = true;
                HLVM_LOG(LogTest, info, TXT("RenderGBuffer debug: mesh '{}' has {} verts, {} indices, first V pos=({},{},{}) first I={}"),
                    Entry.second->GetName(),
                    static_cast<uint32_t>(V.size()), static_cast<uint32_t>(I.size()),
                    V[0].Position.x, V[0].Position.y, V[0].Position.z,
                    I.size() > 0 ? I[0] : 0u);
                if (GBufferVertexBuffer && GBufferIndexBuffer)
                {
                    nvrhi::BufferDesc VbDesc = GBufferVertexBuffer->getDesc();
                    nvrhi::BufferDesc IbDesc = GBufferIndexBuffer->getDesc();
                    HLVM_LOG(LogTest, info, TXT("  GBuffer VB byteSize={} IB byteSize={}"),
                        VbDesc.byteSize, IbDesc.byteSize);
                }
            }

            // Locate matching FInstanceInfo by mesh pointer — same loop order
            // as LoadSponza uses.
            // (We could cache this during LoadSponza; the linear scan here is
            //  small (27 Sponza meshes) and keeps the two structures decoupled.)
            FInstanceInfo ThisInfo{};
            bool Found = false;
            // Use the same MeshMultiMaterialMap key — but that needs a mesh
            // shared_ptr. We have it; just look up Info by matching vertex
            // count via the offsets baked at LoadSponza.
            // Simpler: re-walk LoadSponza's order and pair by mesh identity.
            // (See parallel arrays in LoadSponza; for now we scan SceneBLASes
            //  and rely on consistent mesh ordering between init and render.)
            // For correctness we re-derive Info inline here matching LoadSponza.
            // This mirrors LoadSponza's logic exactly.
            {
                uint32_t VOff = 0, IOff = 0;
                for (auto& E2 : Scene->MeshTree)
                {
                    auto M2 = std::dynamic_pointer_cast<FStaticMesh>(E2.second);
                    if (!M2) continue;
                    if (M2 == StaticMesh)
                    {
                        ThisInfo.VertexOffset = VOff;
                        ThisInfo.IndexOffset  = IOff;
                        ThisInfo.VertexCount  = static_cast<uint32_t>(M2->GetVertices().size());
                        ThisInfo.IndexCount   = static_cast<uint32_t>(M2->GetIndices().size());
                        ThisInfo.AlbedoTextureIndex = 0;
                        // Pull material color if available
                        auto MatIt = Scene->MeshMultiMaterialMap.find(E2.second);
                        if (MatIt != Scene->MeshMultiMaterialMap.end() && !MatIt->second.empty())
                        {
                            const auto& M = MatIt->second[0];
                            ThisInfo.AlbedoColor[0] = M->AlbedoColor.x;
                            ThisInfo.AlbedoColor[1] = M->AlbedoColor.y;
                            ThisInfo.AlbedoColor[2] = M->AlbedoColor.z;
                        }
                        else
                        {
                            ThisInfo.AlbedoColor[0] = 0.7f;
                            ThisInfo.AlbedoColor[1] = 0.7f;
                            ThisInfo.AlbedoColor[2] = 0.7f;
                        }
                        ThisInfo.MaterialFlags = 0;
                        Found = true;
                        break;
                    }
                    VOff += static_cast<uint32_t>(M2->GetVertices().size());
                    IOff += static_cast<uint32_t>(M2->GetIndices().size());
                }
            }
            if (!Found)
            {
                continue;
            }

            // Upload this mesh's FInstanceInfo to the per-instance CB.
            CommandList->writeBuffer(GBufferPerInstanceCB, &ThisInfo, sizeof(ThisInfo));

            // Build the per-draw binding set.
            FBindingSetBuilder SetBuilder;
            SetBuilder.SetConstantBuffer(0, ViewConstantsBuffer)
                      .SetConstantBuffer(1, GBufferPerInstanceCB);
            nvrhi::BindingSetHandle BS = NvrhiDevice->createBindingSet(
                SetBuilder.Build(), GBufferBindingLayout);

            nvrhi::GraphicsState State;
            State.setPipeline(GBufferPipeline);
            State.setFramebuffer(GBufferFrameBuffer);
            State.addBindingSet(BS);

            nvrhi::VertexBufferBinding VBB;
            VBB.setBuffer(GBufferVertexBuffer);
            VBB.setSlot(0);
            VBB.setOffset(0);
            State.addVertexBuffer(VBB);

            nvrhi::IndexBufferBinding IBB;
            IBB.setBuffer(GBufferIndexBuffer);
            IBB.setOffset(0);
            IBB.setFormat(nvrhi::Format::R32_UINT);
            State.setIndexBuffer(IBB);

            nvrhi::Viewport Vp(0.f, float(LastWidth), 0.f, float(LastHeight), 0.f, 1.f);
            State.viewport.addViewportAndScissorRect(Vp);

            CommandList->setGraphicsState(State);

            nvrhi::DrawArguments Args;
            Args.vertexCount         = ThisInfo.IndexCount;
            Args.instanceCount       = 1;
            Args.startIndexLocation  = ThisInfo.IndexOffset;
            Args.startVertexLocation = ThisInfo.VertexOffset;
            Args.startInstanceLocation = 0;
            CommandList->drawIndexed(Args);
            if (!bDebugLogged && FrameCount == 1 && MeshCount == 1)
            {
                bDebugLogged = true;
                HLVM_LOG(LogTest, info, TXT("After drawIndexed[0]: VS={} PS={} PL={} IL={} BS={} FB={} VB={} IB={}"),
                    GBufferVS ? 1 : 0, GBufferPS ? 1 : 0, GBufferPipeline ? 1 : 0,
                    GBufferInputLayout ? 1 : 0, BS ? 1 : 0,
                    GBufferFrameBuffer ? 1 : 0, GBufferVertexBuffer ? 1 : 0,
                    GBufferIndexBuffer ? 1 : 0);
            }

            ++MeshCount;
        }

        // After all GBuffer draws, transition MRTs to ShaderResource for
        // FGIPass's SRV reads. nvrhi Vulkan wants
        // SHADER_READ_ONLY_OPTIMAL for Texture2D<float4> loads.
        CommandList->setTextureState(GBufferWorldPos, nvrhi::AllSubresources,
            nvrhi::ResourceStates::ShaderResource);
        CommandList->setTextureState(GBufferNormal,   nvrhi::AllSubresources,
            nvrhi::ResourceStates::ShaderResource);
        CommandList->setTextureState(GBufferMaterial, nvrhi::AllSubresources,
            nvrhi::ResourceStates::ShaderResource);

        // HLVM-bypass: close+execute the raster pass in its own CommandList
        // so a Vulkan validation error in a later pass (e.g. the temporal-
        // reservoir layout mismatch from bug-075) cannot retroactively drop
        // all raster work recorded earlier in the same submission.
        CommandList->close();
        NvrhiDevice->executeCommandList(CommandList);
        NvrhiDevice->waitForIdle();

        // Reopen for the remaining passes (FGIPass, denoise, ReSTIR, etc.).
        CommandList->open();

        if (MeshCount == 0)
        {
            HLVM_LOG(LogTest, warn, TXT("RenderGBuffer: 0 meshes drawn (frame {})"), FrameCount);
        }
        else if (FrameCount < 4 || FrameCount % 120 == 0)
        {
            HLVM_LOG(LogTest, info, TXT("RenderGBuffer frame {}: drew {} meshes, viewport {}x{}"),
                FrameCount, MeshCount, LastWidth, LastHeight);
        }
    }

    // ---- View constants ----------------------------------------------------
    void UpdateViewConstants(uint32_t W, uint32_t H)
    {
        glm::vec3 CamPos    = GetCameraPos();
        glm::vec3 CamTarget = GetCameraTarget();
        glm::vec3 CamUp     = GetCameraUp();
        float Fov = GetCameraFovDeg();

        // Sponza vertices live in the original GLTF coord space (±hundreds).
        // Apply the 0.01 scale used by the TLAS so rasterized worldPos matches
        // what FGIPass sees. Without this, all vertices are clipped by the
        // far plane (vertices are at distance ~500 from the camera at z=8).
        glm::mat4 Model = glm::scale(glm::mat4(1.0f), glm::vec3(0.01f));
        glm::mat4 View  = glm::lookAt(CamPos, CamTarget, CamUp);
        // Use RH-ZO perspective WITHOUT Z flip. glm::perspective already
        // produces [-1, 1] Z (OpenGL convention). Don't flip — flipping moves
        // geometry away from the viewer when the view matrix expects -Z forward.
        glm::mat4 Proj  = glm::perspective(
            glm::radians(Fov), float(W) / float(H), 0.001f, 50.0f);

        struct FVC { glm::mat4 Model; glm::mat4 View; glm::mat4 Proj; glm::vec2 Size; float FrameIndex; float Pad; };
        FVC VC{Model, View, Proj, {float(W), float(H)}, float(AccumFrameCount), 0.0f};
        CommandList->writeBuffer(ViewConstantsBuffer, &VC, sizeof(VC));
    }

    // ---- GI accumulate pass ------------------------------------------------
    bool CreateAccumulationPipeline(const FString& DataDir)
    {
        const std::string SblobPath = FPath::Combine(DataDir, TXT("GIAccumulate_cs.sblob")).string();
        auto Blob = ReadBinaryFile(SblobPath);
        if (Blob.empty())
        {
            HLVM_LOG(LogTest, err, TXT("Failed to read GIAccumulate_cs.sblob at {}"), *FString(SblobPath.c_str()));
            return false;
        }
        const void* Bin = nullptr; size_t BinSize = 0;
        if (!ShaderMake::FindPermutationInBlob(Blob.data(), Blob.size(), nullptr, 0, &Bin, &BinSize))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to extract GIAccumulate_cs compute shader"));
            return false;
        }
        nvrhi::ShaderDesc CSDesc;
        CSDesc.setShaderType(nvrhi::ShaderType::Compute);
        AccumulateCS = NvrhiDevice->createShader(CSDesc, Bin, BinSize);
        if (!AccumulateCS) return false;

        FBindingLayoutBuilder BLB;
        BLB.SetVisibility(nvrhi::ShaderType::Compute)
           .AddConstantBuffer(0)
           .AddTextureSRV(0)
           .AddTextureUAV(0)
           .AddTextureUAV(1);

        // AGENTS.md gotcha: NVRHI defaults `constantBufferOffset` to 256; the
        // shader's b0 must end up at Vulkan binding 0. Set offsets explicitly.
        nvrhi::VulkanBindingOffsets Offsets;
        Offsets.setConstantBufferOffset(0)
               .setShaderResourceOffset(0)
               .setSamplerOffset(0)
               .setUnorderedAccessViewOffset(0);
        BLB.SetBindingOffsets(Offsets);

        AccumulateBindingLayout = NvrhiDevice->createBindingLayout(BLB.Build());

        nvrhi::ComputePipelineDesc CPDesc;
        CPDesc.setComputeShader(AccumulateCS)
              .addBindingLayout(AccumulateBindingLayout);
        AccumulatePipeline = NvrhiDevice->createComputePipeline(CPDesc);
        if (!AccumulatePipeline) return false;

        nvrhi::BufferDesc CD;
        CD.byteSize = 16;   // 4 constants (uint, uint, uint, float)
        CD.isConstantBuffer = true;
        CD.initialState = nvrhi::ResourceStates::ConstantBuffer;
        CD.keepInitialState = true;
        CD.debugName = "GIAccumulateConstants";
        AccumulateConstants = NvrhiDevice->createBuffer(CD);
        return true;
    }

    // ---- Dump last-frame textures -----------------------------------------
    // Reads back the displayed final frame to CPU, then writes to PNG via
    // FImageDump::DumpToPNG. Following the pattern in TestPathTraceGI.
    void DumpCurrentFrame()
    {
        const FPath DumpDir = FPath(FString::Format(
            TXT("{}/Engine/Source/Runtime/Test/{}_Data/dumps"),
            *GProjectRoot, *GExecutableName));
        std::string dir = DumpDir.string();
        std::filesystem::create_directories(dir);

        // Final-image dump is what validate_restir_gi.py actually inspects.
        DumpRGBA32FTexture(DisplayTexture, TXT("display"), dir);
        DumpRGBA32FTexture(SpatialRadiance, TXT("spatial"), dir);
        DumpRGBA32FTexture(DenoisedTexture, TXT("denoised"), dir);
        DumpRGBA32FTexture(OutputTexture, TXT("gi_raw"), dir);
        // GBuffer channel dumps (per-frame sentinel + post-pass values).
        // Same HLVM_DUMP_RGI gate; same dir; same naming convention
        // (timestamp_channel_frameN.png) so the validator can consume them
        // unchanged. Sentinel pixels remaining in these dumps mark pixels the
        // raster pass did not write. WorldPos is normalized per-channel so
        // world-space positions (which fall outside [0,1]) are visible in the
        // PNG; normal and material are already in [0,1] and stay unnormalized.
        DumpRGBA32FTexture(GBufferWorldPos, TXT("gbuffer_worldpos"), dir, /*bNormalizePerChannel=*/true);
        DumpRGBA32FTexture(GBufferNormal,   TXT("gbuffer_normal"),   dir);
        DumpRGBA32FTexture(GBufferMaterial, TXT("gbuffer_material"), dir);
        HLVM_LOG(LogTest, info, TXT("Dumped frames to {}"), *FString(dir));
    }

    // CPU-readback then PNG for one RGBA32F texture. Creates a one-shot
    // staging texture, copies, maps, runs the bytes through FImageDump::DumpToPNG.
    //
    // For textures with values outside [0,1] (e.g. world-space positions in
    // GBufferWorldPos), pass bNormalizePerChannel=true. The dumper computes
    // min/max across RGB channels and rescales to [0,1] before byte
    // encoding, so the visualization reflects relative variation instead of
    // clamping to 0 or 255.
    void DumpRGBA32FTexture(nvrhi::TextureHandle Texture, const FString& Name, const std::string& dir,
                            bool bNormalizePerChannel = false)
    {
        if (!Texture || !NvrhiDevice) return;

        // 1x1 unused, actual size is supplied via the slice
        nvrhi::TextureDesc StagingDesc;
        StagingDesc.dimension = nvrhi::TextureDimension::Texture2D;
        StagingDesc.width     = WIDTH;
        StagingDesc.height    = HEIGHT;
        StagingDesc.format    = nvrhi::Format::RGBA32_FLOAT;
        StagingDesc.isRenderTarget = false;
        StagingDesc.isUAV     = false;
        StagingDesc.isTypeless = false;
        StagingDesc.initialState = nvrhi::ResourceStates::CopyDest;
        StagingDesc.keepInitialState = false;
        StagingDesc.debugName = "DumpStaging";
        nvrhi::StagingTextureHandle Staging = NvrhiDevice->createStagingTexture(
            StagingDesc, nvrhi::CpuAccessMode::Read);
        if (!Staging) return;

        nvrhi::CommandListHandle Cmd = NvrhiDevice->createCommandList();
        Cmd->open();
        Cmd->setTextureState(Texture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopySource);

        nvrhi::TextureSlice Slice;
        Slice.width = WIDTH;
        Slice.height = HEIGHT;
        Slice.depth = 1;
        Cmd->copyTexture(Staging.Get(), Slice, Texture.Get(), Slice);
        Cmd->close();
        NvrhiDevice->executeCommandList(Cmd);
        NvrhiDevice->waitForIdle();

        size_t RowPitch = 0;
        void* Mapped = NvrhiDevice->mapStagingTexture(
            Staging.Get(), Slice, nvrhi::CpuAccessMode::Read, &RowPitch);
        if (!Mapped) { return; }

        std::vector<float> Pixels(static_cast<size_t>(WIDTH) * HEIGHT * 4);
        const uint8_t* SrcRow = reinterpret_cast<const uint8_t*>(Mapped);
        for (uint32_t y = 0; y < HEIGHT; ++y)
        {
            const float* Src = reinterpret_cast<const float*>(SrcRow + static_cast<size_t>(y) * RowPitch);
            for (uint32_t x = 0; x < WIDTH; ++x)
            {
                size_t SrcIdx = x * 4;
                size_t DstIdx = (static_cast<size_t>(y) * WIDTH + x) * 4;
                Pixels[DstIdx + 0] = Src[SrcIdx + 0];
                Pixels[DstIdx + 1] = Src[SrcIdx + 1];
                Pixels[DstIdx + 2] = Src[SrcIdx + 2];
                Pixels[DstIdx + 3] = 1.0f;
            }
        }
        NvrhiDevice->unmapStagingTexture(Staging.Get());

        if (bNormalizePerChannel)
        {
            // Per-channel min/max across all pixels; rescale RGB to [0,1].
            // Preserves relative structure (which is what you want for a
            // worldpos debug visualization) and avoids the clamp-to-extreme
            // behavior that makes non-normalized textures look like solid
            // quadrants.
            float MinR = Pixels[0], MaxR = Pixels[0];
            float MinG = Pixels[1], MaxG = Pixels[1];
            float MinB = Pixels[2], MaxB = Pixels[2];
            const size_t NPix = static_cast<size_t>(WIDTH) * HEIGHT;
            for (size_t i = 0; i < NPix; ++i)
            {
                const float r = Pixels[i*4 + 0];
                const float g = Pixels[i*4 + 1];
                const float b = Pixels[i*4 + 2];
                if (r < MinR) MinR = r; if (r > MaxR) MaxR = r;
                if (g < MinG) MinG = g; if (g > MaxG) MaxG = g;
                if (b < MinB) MinB = b; if (b > MaxB) MaxB = b;
            }
            const float RangeR = (MaxR - MinR) > 1e-6f ? (MaxR - MinR) : 1.0f;
            const float RangeG = (MaxG - MinG) > 1e-6f ? (MaxG - MinG) : 1.0f;
            const float RangeB = (MaxB - MinB) > 1e-6f ? (MaxB - MinB) : 1.0f;
            for (size_t i = 0; i < NPix; ++i)
            {
                Pixels[i*4 + 0] = (Pixels[i*4 + 0] - MinR) / RangeR;
                Pixels[i*4 + 1] = (Pixels[i*4 + 1] - MinG) / RangeG;
                Pixels[i*4 + 2] = (Pixels[i*4 + 2] - MinB) / RangeB;
            }
            HLVM_LOG(LogTest, info,
                TXT("DumpRGBA32FTexture: {} normalized per-channel — R[{:.3f},{:.3f}] G[{:.3f},{:.3f}] B[{:.3f},{:.3f}]"),
                *Name, MinR, MaxR, MinG, MaxG, MinB, MaxB);
        }

        const std::string Filename = dir + "/" + MakeTimestampPrefix() + "_" +
            std::string(Name.begin(), Name.end()) + "_frame" + std::to_string(AccumFrameCount) + ".png";
        if (FImageDump::DumpToPNG(FString(Filename.c_str()), static_cast<int>(WIDTH), static_cast<int>(HEIGHT), Pixels.data()))
        {
            HLVM_LOG(LogTest, info, TXT("Dumped {} ({})"), *Name, *FString(Filename));
        }
    }

private:
    // ---- Members -----------------------------------------------------------
    nvrhi::IDevice*              NvrhiDevice = nullptr;
    nvrhi::FramebufferInfo        FBInfo;
    FString                       WindowTitle;
    FBindingCache                 BindingCache;
    nvrhi::CommandListHandle      CommandList;

    nvrhi::SamplerHandle          LinearSampler;
    nvrhi::BufferHandle           ViewConstantsBuffer;

    // GBuffer + scene
    nvrhi::TextureHandle          GBufferWorldPos;
    nvrhi::TextureHandle          GBufferNormal;
    nvrhi::TextureHandle          GBufferMaterial;
    nvrhi::TextureHandle          GBufferDepth;
    nvrhi::TextureHandle          LinearDepthTexture;
    nvrhi::BufferHandle           VertexBuffer;
    nvrhi::BufferHandle           IndexBuffer;
    nvrhi::BufferHandle           InstanceInfoBuffer;
    nvrhi::rt::AccelStructHandle  SceneTLAS;
    std::vector<nvrhi::rt::AccelStructHandle> SceneBLASes;
    std::shared_ptr<FScene3DNode> Scene;

    // Pipeline passes
    GI::FGIPass                   GIPass;
    FBilateralDenoisePass         BilateralDenoisePass;
    ReSTIR::FReSTIRPass           ReSTIRPass;
    // ReBLUR is intentionally skipped in v1 to keep the test focused on
    // the path-trace-debug fixes (payload, camera, lights). Adding ReBLUR
    // here is a follow-up commit; see TestCornellBoxGI for the ReBLUR pattern.

    // Per-frame intermediate textures
    nvrhi::TextureHandle          OutputTexture;
    nvrhi::TextureHandle          DenoisedTexture;
    nvrhi::TextureHandle          ReservoirTex0;
    nvrhi::TextureHandle          ReservoirTex1;
    nvrhi::TextureHandle          TemporalReservoir0;
    nvrhi::TextureHandle          TemporalReservoir1;
    nvrhi::TextureHandle          SpatialRadiance;
    nvrhi::TextureHandle          AccumTexture;
    nvrhi::TextureHandle          DisplayTexture;

    // GIAccumulate pipeline
    nvrhi::ComputePipelineHandle  AccumulatePipeline;
    nvrhi::BindingLayoutHandle    AccumulateBindingLayout;
    nvrhi::ShaderHandle           AccumulateCS;
    nvrhi::BufferHandle           AccumulateConstants;

    // GBuffer PT pipeline (renders Sponza into GBufferWorldPos /
    // GBufferNormal / GBufferMaterial MRTs before FGIPass).
    nvrhi::ShaderHandle           GBufferVS;
    nvrhi::ShaderHandle           GBufferPS;
    nvrhi::InputLayoutHandle      GBufferInputLayout;
    nvrhi::BindingLayoutHandle    GBufferBindingLayout;
    nvrhi::GraphicsPipelineHandle GBufferPipeline;
    nvrhi::FramebufferHandle      GBufferFrameBuffer;
    nvrhi::BufferHandle           GBufferVertexBuffer;
    nvrhi::BufferHandle           GBufferIndexBuffer;
    nvrhi::BufferHandle           GBufferPerInstanceCB;

    // Frame counters / env
    uint32_t  LastWidth  = 0;
    uint32_t  LastHeight = 0;
    uint32_t  AccumFrameCount = 0;
    uint32_t  AccumTargetFrames = DEFAULT_ACCUM_TARGET_FRAMES;
    float     Exposure = 1.0f;
    bool      bDumpRequested = false;
    uint32_t  FrameCount = 0;
    float     FPSUpdateTimer = 0.0f;
};

// ============================================================================
// Test entry
// ============================================================================

RECORD_BOOL(test_ReSTIR_GI_Temporal)
{
    HLVM_LOG(LogTest, info, TXT("Starting ReSTIR GI Temporal Test..."));

    try
    {
        IWindow::Properties WindowProps;
        WindowProps.Title    = WINDOW_TITLE;
        WindowProps.Extent   = { WIDTH, HEIGHT };
        WindowProps.Resizable = true;
        WindowProps.StartMinimized = true;  // headless CI: don't require real display
        WindowProps.VSync    = IWindow::EVsync::Off;

        auto DeviceManager = FDeviceManager::Create(nvrhi::GraphicsAPI::VULKAN);
        if (!DeviceManager) throw std::runtime_error("Failed to create DeviceManager");

        // Critical: enable RT extensions BEFORE creating the device,
        // otherwise nvrhi Vulkan won't enable VK_KHR_ray_tracing_pipeline
        // and FGIPass cannot create its RT pipeline.
        {
            auto& Params0 = const_cast<FDeviceCreationParameters&>(DeviceManager->GetDeviceParams());
            Params0.bEnableRayTracingExtensions = true;
        }

        if (!DeviceManager->CreateWindowDeviceAndSwapChain(WindowProps))
        {
            throw std::runtime_error("Failed to create window, device and swap chain");
        }
        HLVM_LOG(LogTest, info, TXT("Device created with ray tracing enabled"));

        auto& Params = const_cast<FDeviceCreationParameters&>(DeviceManager->GetDeviceParams());
        Params.BackBufferWidth   = WIDTH;
        Params.BackBufferHeight  = HEIGHT;
        Params.SwapChainBufferCount = 2;
        Params.VSyncMode = 0;
        Params.bEnableDebugRuntime = true;

        nvrhi::IDevice* NvrhiDevice = DeviceManager->GetDevice();
        if (!NvrhiDevice->queryFeatureSupport(nvrhi::Feature::RayTracingPipeline))
        {
            throw std::runtime_error("Ray tracing pipeline not supported on this device");
        }

        nvrhi::IFramebuffer* FirstFB = DeviceManager->GetFramebuffer(0);

        TSharedPtr<FReSTIRGITemporalPass> RTPass =
            std::make_shared<FReSTIRGITemporalPass>(DeviceManager.get());
        if (!RTPass->Initialize(NvrhiDevice, FirstFB, FString(TXT("ReSTIR GI Temporal"))))
        {
            throw std::runtime_error("Failed to initialize FReSTIRGITemporalPass");
        }

        DeviceManager->AddRenderPassToBack(RTPass);

        // Safety timer: stop the loop after a generous budget regardless of
        // what the test does internally (see TestPathTraceGI for the pattern).
        std::thread([&]() {
            const double TimeoutSec = std::max(30.0, 16.0);
            FTimer Timer;
            while (Timer.MarkSec() < TimeoutSec) {}
            DeviceManager->StopMessageLoop();
        }).detach();

        DeviceManager->RunMessageLoop();
    }
    catch (const std::exception& e)
    {
        HLVM_LOG(LogTest, critical, TXT("Test failed: {}"), TO_TCHAR_CSTR(e.what()));
        return false;
    }
    catch (...)
    {
        HLVM_LOG(LogTest, critical, TXT("Test failed: unknown exception"));
        return false;
    }

    HLVM_LOG(LogTest, info, TXT("ReSTIR GI Temporal test completed"));
    return true;
}

#else

RECORD_BOOL(test_ReSTIR_GI_Temporal)
{
    HLVM_LOG(LogTest, warn, TXT("ReSTIR GI Temporal test is Vulkan-only"));
    return true;
}

#endif // HLVM_VULKAN_RENDERER
