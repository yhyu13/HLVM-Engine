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
        nvrhi::TextureHandle DepthTexture;   // Depth for edge detection
        nvrhi::TextureHandle NormalTexture; // Normals for edge detection (optional)
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
