#pragma once

#include "Core/String.h"
#include <nvrhi/nvrhi.h>

namespace TAA
{
    struct FTAAConstants
    {
        TFP32 InverseCurrViewProj[16];
        TFP32 PrevViewProj[16];
        TFP32 OutputSize[2];
        TFP32 RcpOutputSize[2];
        TFP32 BlendFactor;
        TFP32 DepthThreshold;
        TFP32 Pad[2];
    };
    static_assert(sizeof(FTAAConstants) == 160, "FTAAConstants must be 160 bytes");

    class FTAAPass
    {
    public:
        struct FDesc
        {
            nvrhi::TextureHandle CurrentFrameTexture;
            nvrhi::TextureHandle HistoryFrameTexture;
            nvrhi::TextureHandle DepthTexture;
            nvrhi::TextureHandle OutputTexture;
            uint32_t OutputWidth = 0;
            uint32_t OutputHeight = 0;
        };

        FTAAPass() = default;
        ~FTAAPass() { Shutdown(); }

        FTAAPass(const FTAAPass&) = delete;
        FTAAPass& operator=(const FTAAPass&) = delete;
        FTAAPass(FTAAPass&&) = delete;
        FTAAPass& operator=(FTAAPass&&) = delete;

        bool Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir);
        void Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FTAAConstants& Constants);
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
}
