#pragma once

#include "Core/String.h"
#include <nvrhi/nvrhi.h>

namespace SSr
{
    struct FSSRConstants
    {
        TFP32 ProjMatrix[16];
        TFP32 InvProjMatrix[16];
        TFP32 ViewMatrix[16];
        TFP32 FullScreenSize[2];
        TFP32 InvFullScreenSize[2];
        TFP32 HalfScreenSize[2];
        TFP32 InvHalfScreenSize[2];
        TINT32 MaxSteps;
        TFP32 StepSize;
        TFP32 MaxDistance;
        TFP32 Thickness;
        TFP32 Pad0[4];
    };
    static_assert(sizeof(FSSRConstants) == 256, "FSSRConstants must be 256 bytes");

    class FSSRPass
    {
    public:
        struct FDesc
        {
            nvrhi::TextureHandle DepthTexture;
            nvrhi::TextureHandle NormalTexture;
            nvrhi::TextureHandle MaterialTexture;
            nvrhi::TextureHandle HDRTexture;
            nvrhi::TextureHandle OutputTexture;
            uint32_t OutputWidth = 0;
            uint32_t OutputHeight = 0;
        };

        FSSRPass() = default;
        ~FSSRPass() { Shutdown(); }

        FSSRPass(const FSSRPass&) = delete;
        FSSRPass& operator=(const FSSRPass&) = delete;
        FSSRPass(FSSRPass&&) = delete;
        FSSRPass& operator=(FSSRPass&&) = delete;

        bool Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir);
        void Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FSSRConstants& Constants);
        void Shutdown();

    private:
        nvrhi::IDevice* Device = nullptr;
        nvrhi::ShaderHandle ComputeShader;
        nvrhi::BindingLayoutHandle BindingLayout;
        nvrhi::ComputePipelineHandle Pipeline;
        nvrhi::BufferHandle ConstantBuffer;
        nvrhi::SamplerHandle PointSampler;
        nvrhi::SamplerHandle LinearSampler;
        FString ShaderDataDir;
        bool bIsInitialized = false;
    };
}
