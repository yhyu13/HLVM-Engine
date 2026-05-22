#pragma once

#include "Core/String.h"
#include <nvrhi/nvrhi.h>

namespace Exposure
{
    struct FExposureConstants
    {
        TFP32 AdaptationSpeed;
        TFP32 KeyValue;
        TFP32 Pad[2];
    };
    static_assert(sizeof(FExposureConstants) == 16, "FExposureConstants must be 16 bytes");

    class FExposureAdaptationPass
    {
    public:
        struct FDesc
        {
            nvrhi::TextureHandle SceneColorTexture;
            nvrhi::TextureHandle AdaptedLuminanceTexture;
        };

        FExposureAdaptationPass() = default;
        ~FExposureAdaptationPass() { Shutdown(); }

        FExposureAdaptationPass(const FExposureAdaptationPass&) = delete;
        FExposureAdaptationPass& operator=(const FExposureAdaptationPass&) = delete;
        FExposureAdaptationPass(FExposureAdaptationPass&&) = delete;
        FExposureAdaptationPass& operator=(FExposureAdaptationPass&&) = delete;

        bool Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir);
        void Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FExposureConstants& Constants);
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
