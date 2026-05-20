#pragma once

#include "Core/String.h"
#include <nvrhi/nvrhi.h>

class FToneMappingPass
{
public:
    struct FDesc
    {
        nvrhi::TextureHandle HDRInputTexture;
        nvrhi::TextureHandle BloomTexture;
        nvrhi::TextureHandle SSRTexture;
        nvrhi::TextureHandle SDROutputTexture;
        uint32_t Width = 0;
        uint32_t Height = 0;
    };

    // Shader cbuffer is 2 registers (32 bytes). Padded to 256 for alignment consistency.
    struct FConstants
    {
        float Exposure;
        float Gamma;
        int32_t TonemapMode;
        float BloomIntensity;
        float TextureSize[2];
        float Pad[2];
        float Pad2[56];
    };
    static_assert(sizeof(FConstants) == 256, "FToneMappingPass constants must be 256 bytes");

    FToneMappingPass() = default;
    ~FToneMappingPass() { Shutdown(); }

    FToneMappingPass(const FToneMappingPass&) = delete;
    FToneMappingPass& operator=(const FToneMappingPass&) = delete;
    FToneMappingPass(FToneMappingPass&&) = delete;
    FToneMappingPass& operator=(FToneMappingPass&&) = delete;

    bool Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir);
    void Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FConstants& Constants);
    void Shutdown();

private:
    nvrhi::IDevice* Device = nullptr;
    nvrhi::ShaderHandle ComputeShader;
    nvrhi::BindingLayoutHandle BindingLayout;
    nvrhi::ComputePipelineHandle Pipeline;
    nvrhi::BufferHandle ConstantBuffer;
    nvrhi::SamplerHandle LinearSampler;
    FString ShaderDataDir;
    bool bIsInitialized = false;
};
