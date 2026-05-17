#pragma once

#include "Core/String.h"

#include <nvrhi/nvrhi.h>

namespace SSao
{
    struct FHBAOConstants
    {
        TFP32 ProjMatrix[16];
        TFP32 InvProjMatrix[16];
        TFP32 ViewMatrix[16];
        TFP32 ScreenSize[2];
        TFP32 InvScreenSize[2];
        TFP32 SampleRadius;
        TFP32 AngleBias;
        TFP32 MaxRadiusPixels;
        TFP32 AttenuationScale;
        TFP32 MinAO;
        TINT32 DirectionCount;
        TINT32 StepCount;
        TFP32 Pad0[2];
        TFP32 Pad1[3];
    };
    static_assert(sizeof(FHBAOConstants) == 256, "FHBAOConstants must be 256 bytes");

    class FSSAOPass
    {
    public:
        struct FDesc
        {
            nvrhi::TextureHandle DepthTexture;
            nvrhi::TextureHandle NormalTexture;
            nvrhi::TextureHandle OutputTexture;
            nvrhi::SamplerHandle PointSampler;
            uint32_t OutputWidth = 0;
            uint32_t OutputHeight = 0;
            float SampleRadius = 1.0f;
            float AngleBias = 0.2f;
            float MaxRadiusPixels = 50.0f;
            float AttenuationScale = 1.0f;
            float MinAO = 0.0f;
            int32_t DirectionCount = 4;
            int32_t StepCount = 6;
        };

        FSSAOPass();
        ~FSSAOPass();

        FSSAOPass(const FSSAOPass&) = delete;
        FSSAOPass& operator=(const FSSAOPass&) = delete;
        FSSAOPass(FSSAOPass&&) = delete;
        FSSAOPass& operator=(FSSAOPass&&) = delete;

        bool Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir);
        void Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FHBAOConstants& Constants);
        void Shutdown();

    private:
        nvrhi::IDevice* Device = nullptr;
        nvrhi::ShaderHandle ComputeShader;
        nvrhi::BindingLayoutHandle BindingLayout;
        nvrhi::ComputePipelineHandle Pipeline;
        nvrhi::BufferHandle ConstantBuffer;
    };
}
