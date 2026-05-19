#pragma once

#include "Core/String.h"
#include <nvrhi/nvrhi.h>

class FBloomPass
{
public:
    struct FDesc
    {
        nvrhi::TextureHandle HDRTexture;
        nvrhi::TextureHandle OutputTexture;
        nvrhi::TextureHandle HalfResTexture;
        nvrhi::TextureHandle BlurTempTexture;
        nvrhi::SamplerHandle LinearSampler;
        uint32_t FullResWidth = 0;
        uint32_t FullResHeight = 0;
        float Threshold = 0.8f;
        float Intensity = 0.4f;
        float Sigma = 2.0f;
        int32_t BlurIterations = 2;
    };

    FBloomPass() = default;
    ~FBloomPass() { Shutdown(); }

    FBloomPass(const FBloomPass&) = delete;
    FBloomPass& operator=(const FBloomPass&) = delete;
    FBloomPass(FBloomPass&&) = delete;
    FBloomPass& operator=(FBloomPass&&) = delete;

    bool Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir);
    void Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc);
    void Shutdown();

private:
    nvrhi::IDevice* Device = nullptr;
    nvrhi::ShaderHandle ThresholdShader;
    nvrhi::ShaderHandle BlurShader;
    nvrhi::ShaderHandle UpsampleShader;
    nvrhi::BindingLayoutHandle BindingLayout;
    nvrhi::ComputePipelineHandle ThresholdPipeline;
    nvrhi::ComputePipelineHandle BlurPipeline;
    nvrhi::ComputePipelineHandle UpsamplePipeline;
    nvrhi::BufferHandle ConstantBuffer;
    FString ShaderDataDir;
};
