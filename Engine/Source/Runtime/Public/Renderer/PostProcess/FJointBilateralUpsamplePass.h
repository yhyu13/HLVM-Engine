// Copyright 2026 HLVM Engine
//
// MIT License
//
// FJointBilateralUpsamplePass - Generic depth-aware edge-preserving upsample filter.
//
// Uses a joint bilateral filter guided by a depth texture to prevent bleeding
// across depth discontinuities. Supports both same-resolution denoising and
// upsampling from lower-resolution inputs.
//
// Typical use cases:
//   - SSAO/SSS blur/denoise (same-resolution)
//   - Upsampling half-resolution bloom, SSR, volumetrics (upscaling)

#pragma once

#include "Core/String.h"
#include <nvrhi/nvrhi.h>

class FJointBilateralUpsamplePass
{
public:
    struct FDesc
    {
        nvrhi::TextureHandle InputTexture;   // Source (can be lower res)
        nvrhi::TextureHandle DepthTexture;   // Edge guide (full-res depth)
        nvrhi::TextureHandle OutputTexture;  // Destination (full-res)
        uint32_t InputWidth = 0;             // 0 = use InputTexture dimensions
        uint32_t InputHeight = 0;
        uint32_t OutputWidth = 0;            // 0 = use OutputTexture dimensions
        uint32_t OutputHeight = 0;
        float DepthSigma = 0.01f;            // Depth difference tolerance
    };

    FJointBilateralUpsamplePass() = default;
    ~FJointBilateralUpsamplePass() { Shutdown(); }

    // Non-copyable
    FJointBilateralUpsamplePass(const FJointBilateralUpsamplePass&) = delete;
    FJointBilateralUpsamplePass& operator=(const FJointBilateralUpsamplePass&) = delete;

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
