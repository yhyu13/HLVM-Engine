#pragma once

#include "Core/String.h"
#include <nvrhi/nvrhi.h>

namespace MotionBlur
{
    struct FMotionBlurConstants
    {
        TFP32 InverseCurrViewProj[16];
        TFP32 PrevViewProj[16];
        TFP32 OutputSize[2];
        TFP32 RcpOutputSize[2];
        TFP32 VelocityScale;
        TFP32 MinVelocity;
        TFP32 Pad[2];
    };
    static_assert(sizeof(FMotionBlurConstants) == 160, "FMotionBlurConstants must be 160 bytes");

    class FMotionBlurPass
    {
    public:
        struct FDesc
        {
            nvrhi::TextureHandle ColorTexture;
            nvrhi::TextureHandle DepthTexture;
            nvrhi::TextureHandle OutputTexture;
            uint32_t OutputWidth = 0;
            uint32_t OutputHeight = 0;
        };

        FMotionBlurPass() = default;
        ~FMotionBlurPass() { Shutdown(); }

        FMotionBlurPass(const FMotionBlurPass&) = delete;
        FMotionBlurPass& operator=(const FMotionBlurPass&) = delete;
        FMotionBlurPass(FMotionBlurPass&&) = delete;
        FMotionBlurPass& operator=(FMotionBlurPass&&) = delete;

        bool Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir);
        void Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FMotionBlurConstants& Constants);
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
