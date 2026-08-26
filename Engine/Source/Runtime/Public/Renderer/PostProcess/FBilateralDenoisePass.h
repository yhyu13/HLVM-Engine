// Copyright 2026 HLVM Engine
//
// MIT License
//
// FBilateralDenoisePass - HDR RGB bilateral denoising pass for GI.
//
// Performs depth-aware and normal-aware edge-preserving filtering for noisy
// few-bounce GI results. Uses a joint bilateral filter guided by depth and
// normals to prevent blurring across geometry and material boundaries.
//
// Typical use case:
//   - Denoise few-bounce GI output (noisy due to limited samples)
//   - Works with HDR RGB input

#pragma once

#include "Core/String.h"
#include <nvrhi/nvrhi.h>

class FBilateralDenoisePass
{
public:
    struct FDesc
    {
        nvrhi::TextureHandle InputTexture;   // Noisy HDR RGB input
        // INVARIANT: DepthTexture and NormalTexture must share one extent.
        // Dispatch derives a single GuideScale (guide width / OutputWidth) and
        // applies it to BOTH guides, so guides of two different extents means
        // one of them is indexed with the other's scale -- no VUID, no error,
        // just silently wrong bilateral weights. Both current consumers create
        // their two guides adjacently from the same extent variable. The scale
        // is derived from DepthTexture, not NormalTexture, because DepthTexture
        // has no null-guard anywhere in the class while the scale derivation
        // must not sit behind a branch a caller can skip (see v205).
        // They need NOT match OutputWidth/Height: the Phase-D consumer
        // dispatches half-res over full-res guides, which is what GuideScale
        // exists for.
        nvrhi::TextureHandle DepthTexture;   // Depth for edge detection (required; sources GuideScale)
        // REQUIRED, despite what this field was documented as until v213. The
        // binding layout declares t2 unconditionally and all three copies of
        // BilateralDenoise_cs.hlsl Load t_Normal twice with no gate, so nothing
        // in this class tolerates a null handle -- it is not optional at any of
        // the levels that could honour the word. Dispatch rejects a null with
        // an error and leaves the output unwritten; it deliberately does not
        // substitute a dummy, because a constant normal makes every bilateral
        // weight identical and degrades the filter to depth-only with no
        // diagnostic. (FReBLURPass CAN use a dummy guide; its shader tolerates
        // one. This one's does not. Do not carry that idiom across.)
        nvrhi::TextureHandle NormalTexture; // Normals for edge detection (required)
        nvrhi::TextureHandle OutputTexture; // Denoised HDR RGB output
        uint32_t OutputWidth = 0;
        uint32_t OutputHeight = 0;
        float DepthSigma = 0.01f;      // Depth difference tolerance
        float NormalSigma = 0.1f;      // Normal difference tolerance (radians)
        float SpatialSigma = 2.0f;     // Spatial weight falloff
    };

    FBilateralDenoisePass() = default;
    ~FBilateralDenoisePass() { Shutdown(); }

    // Non-copyable
    FBilateralDenoisePass(const FBilateralDenoisePass&) = delete;
    FBilateralDenoisePass& operator=(const FBilateralDenoisePass&) = delete;

    bool Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir);
    void Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc);
    void Shutdown();

private:
    nvrhi::IDevice* Device = nullptr;
    nvrhi::ShaderHandle Shader;
    nvrhi::BindingLayoutHandle BindingLayout;
    nvrhi::ComputePipelineHandle Pipeline;
    nvrhi::BufferHandle ConstantBuffer;
    nvrhi::SamplerHandle PointSampler;
    FString ShaderDataDir;
};
