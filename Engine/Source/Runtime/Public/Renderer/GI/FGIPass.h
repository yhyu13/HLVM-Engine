// FGIPass.h - Few-bounce / path-tracing GI ray-tracing pass.
//
// Replaces the inline few-bounce GI raytracing that previously lived in
// TestFewBounceGI.cpp. Mirrors the shape of FBloomPass / FSSAOPass:
//   - Header-only public API
//   - Five copy/move ops deleted (owns nvrhi handles)
//   - Initialize / Dispatch / Shutdown lifecycle
//
// The pipeline wraps FRayTracingPipeline for the RT boilerplate (shader table,
// binding layout, hit groups). The actual ray-tracing math lives in
// Engine/Source/Runtime/Private/Renderer/Shader/GI/{GIRayGen,GIClosestHit,
// GIMiss,GIPathTracing}.hlsl.

#pragma once

#include "Core/String.h"
#include "Renderer/RayTracing/FRayTracingPipeline.h"
#include <nvrhi/nvrhi.h>

#include "Renderer/Scene3D/FScene.h"

namespace GI
{
    // Per-frame input bundle.
    struct FGIPassDesc
    {
        nvrhi::TextureHandle GBufferWorldPos;
        nvrhi::TextureHandle GBufferNormal;
        nvrhi::TextureHandle GBufferMaterial;   // .rgb = albedo, .a = roughness
        nvrhi::SamplerHandle LinearSampler;
        nvrhi::BufferHandle  ViewConstants;     // b0 (FViewConstants)
        nvrhi::rt::AccelStructHandle SceneTLAS; // t0 (SceneBVH)
        nvrhi::TextureHandle OutputTexture;     // u0 (radiance)
        nvrhi::TextureHandle DebugStatsTexture; // u1 (optional, see FGIPassStats)
        nvrhi::TextureHandle OutputDirection;   // u2 (optional; primary sample ray direction for ReSTIR GI reservoirs)
        // v210+ (2026-08-21, ZetaRay ground-truth port, Phase 1): ReSTIR GI
        // candidate-sample state. Both optional — absent -> 1x1 dummy UAVs so
        // TestPathTraceGI / TestCornellBoxGI keep working unchanged.
        //   u3 = SampleInfo: float4(x2Normal.xyz, samplePdf)
        //   u4 = DirectTexture: float4(primaryDirect + primaryAmbient + skyOnPrimaryMiss, 0)
        nvrhi::TextureHandle SampleInfoTexture; // u3 (optional; x2 normal + primary sample PDF)
        nvrhi::TextureHandle DirectTexture;     // u4 (optional; primary direct+ambient, for the display combine)

        // UAV EXTENT CONTRACT (v207). OutputTexture is mandatory and MUST be
        // sized to OutputWidth x OutputHeight: the ray-generation shader stores
        // to u0 and u2 at the raw dispatch coordinate, with no extent guard.
        //   - u2 (OutputDirection) is optional; when absent it falls back to
        //     OutputTexture, so the requirement above is what keeps that write
        //     in bounds. A supplied OutputDirection must itself be at least the
        //     dispatch extent.
        //   - u1 (DebugStatsTexture) is exempt: its shader write is guarded by
        //     the DebugBounceStats flag, so a 1x1 dummy is never written.
        // Note this is the OPPOSITE arrangement to the guide SRVs (t1..t3),
        // which are read through gbScale and therefore need NOT match the
        // dispatch extent — that scale exists precisely so a half-res dispatch
        // can read a full-res GBuffer. Writes are pinned; reads are scaled.

        // Lights array for Next Event Estimation (NEE)
        nvrhi::BufferHandle LightsBuffer;
        uint32_t            LightCount = 0;

        // Per-texel bounce albedo textures (material rework Phase 3b): slots
        // t9..t40, indexed by RTInstanceInfo.AlbedoTextureIndex in the
        // closest-hit shader. Empty → white placeholders (average-albedo
        // fallback path in the shader).
        TVector<nvrhi::TextureHandle> MaterialTextures;

        // RT geometry (vertex/index/instance buffers for closest-hit barycentric lookup)
        nvrhi::BufferHandle RTVertices;
        nvrhi::BufferHandle RTIndices;
        nvrhi::BufferHandle RTInstanceInfo;

        uint32_t OutputWidth  = 0;
        uint32_t OutputHeight = 0;

        // CVars are read at DispatchRays time so runtime tuning works without re-init.
        uint32_t MaxBounces      = 4;
        uint32_t SamplesPerPixel = 8;
        float    MinRayLength    = 0.001f;
        bool     EnableRR        = true;
        float    RussianRoulette = 0.95f;
        bool     DebugBounceStats = false;
        uint32_t FrameIndex       = 0; // seeds RNG so bounces diverge frame-to-frame
        float    AmbientScale     = -1.0f; // < 0 = use CVar r_GI_AmbientScale; 0 disables the fake ambient term

        // v140: expose AmbientColor so callers (notably TestReSTIR_GI_Temporal) can override
        // the hardcoded fallback in FGIPass::WriteConstants. Default matches the existing
        // hardcoded value at FGIPass.cpp:447 for backward-compat with TestPathTraceGI.
        float    AmbientColor[4]  = { 0.6f, 0.6f, 0.65f, 0.0f };
    };

    // Output from DebugStatsTexture readback (u1 UAV written when DebugBounceStats=true).
    // Packed as 4 floats per pixel by GIRayGen; one pixel per (uint2(0,0)) write.
    struct FGIPassStats
    {
        float avgBounces;        // mean bounces-per-ray across all SPP
        float rrSurvivalRate;    // alive-samples / total-samples
        float primaryHits;       // fraction of primary rays that hit geometry
        float avgRadiance;       // mean output luminance (before tonemap)
    };

    class FGIPass
    {
    public:
        FGIPass() = default;
        ~FGIPass() { Shutdown(); }

        FGIPass(const FGIPass&) = delete;
        FGIPass& operator=(const FGIPass&) = delete;
        FGIPass(FGIPass&&) = delete;
        FGIPass& operator=(FGIPass&&) = delete;

        bool Initialize(nvrhi::IDevice* InDevice,
                        const FString& InShaderDataDir,
                        const FScene* InScene = nullptr);
        void DispatchRays(nvrhi::ICommandList* CmdList, const FGIPassDesc& Desc);
        void Shutdown();

        [[nodiscard]] nvrhi::TextureHandle GetLastOutputTexture() const { return OutputTexture; }
        [[nodiscard]] bool IsInitialized() const { return bIsInitialized; }
        [[nodiscard]] const FGIPassStats& GetLastFrameStats() const { return LastFrameStats; }

    private:
        bool LoadShaders();
        bool CreatePipeline();
        bool CreateBindingLayout();
        bool CreateConstantBuffer();
        bool UploadLights();

        void WriteConstants(nvrhi::ICommandList* CmdList, const FGIPassDesc& Desc);

        nvrhi::IDevice* Device = nullptr;
        const FScene* Scene = nullptr;
        FString ShaderDataDir;

        nvrhi::ShaderLibraryHandle ShaderLibrary;
        FRayTracingPipeline RTPipeline;          // RT pipeline wrapper (owns shader table + binding layout)
        nvrhi::BindingLayoutHandle BindingLayout; // cached from RTPipeline for per-frame binding set creation
        // 2026-08-16 (six-role-pipeline v2): v22 split reverted. SRV + UAV
        // share a single binding set. UAVBindingLayout is removed.
        nvrhi::BufferHandle ConstantBuffer;
        nvrhi::BufferHandle LightsBuffer;      // internal lights buffer (synthesized if Desc.LightsBuffer is null)
        uint32_t            LightsCount = 0;

        nvrhi::TextureHandle OutputTexture; // last output (for debugging / test exposure)
        nvrhi::TextureHandle DummyDebugStatsTexture; // 1x1 fallback when debug UAV not requested
        nvrhi::TextureHandle DummySampleInfoTexture; // 1x1 fallback when u3 not supplied (v210)
        nvrhi::TextureHandle DummyDirectTexture;     // 1x1 fallback when u4 not supplied (v210)
        // No u2 counterpart: per the UAV extent contract above, u2 falls back to
        // OutputTexture, not to a 1x1 dummy. Do not add one back by symmetry.
        nvrhi::TextureHandle MaterialPlaceholderTexture; // 1x1 white (Phase 3b)

        FGIPassStats LastFrameStats{};
        bool bIsInitialized = false;
    };
} // namespace GI
