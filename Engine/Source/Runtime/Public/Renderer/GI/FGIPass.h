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

class FScene;

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
                        const FScene* InScene);
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

        void WriteConstants(nvrhi::ICommandList* CmdList, const FGIPassDesc& Desc);

        nvrhi::IDevice* Device = nullptr;
        const FScene* Scene = nullptr;
        FString ShaderDataDir;

        nvrhi::ShaderLibraryHandle ShaderLibrary;
        FRayTracingPipeline RTPipeline;          // RT pipeline wrapper (owns shader table + binding layout)
        nvrhi::BindingLayoutHandle BindingLayout; // cached from RTPipeline for per-frame binding set creation
        nvrhi::BufferHandle ConstantBuffer;

        nvrhi::TextureHandle OutputTexture; // last output (for debugging / test exposure)

        FGIPassStats LastFrameStats{};
        bool bIsInitialized = false;
    };
} // namespace GI
