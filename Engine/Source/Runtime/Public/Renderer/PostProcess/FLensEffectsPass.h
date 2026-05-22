#pragma once

#include "Core/String.h"
#include <nvrhi/nvrhi.h>

namespace LensEffects
{
    struct FLensEffectsConstants
    {
        TFP32 ChromaticAmount;
        TFP32 VignetteIntensity;
        TFP32 GrainIntensity;
        TINT32 FrameIndex;
        TFP32 OutputSize[2];
        TFP32 RcpOutputSize[2];
        TFP32 Pad[4];
    };
    static_assert(sizeof(FLensEffectsConstants) == 48, "FLensEffectsConstants must be 48 bytes");

    class FLensEffectsPass
    {
    public:
        struct FDesc
        {
            nvrhi::TextureHandle SDRTexture;
            nvrhi::TextureHandle OutputTexture;
            uint32_t OutputWidth = 0;
            uint32_t OutputHeight = 0;
        };

        FLensEffectsPass() = default;
        ~FLensEffectsPass() { Shutdown(); }

        FLensEffectsPass(const FLensEffectsPass&) = delete;
        FLensEffectsPass& operator=(const FLensEffectsPass&) = delete;
        FLensEffectsPass(FLensEffectsPass&&) = delete;
        FLensEffectsPass& operator=(FLensEffectsPass&&) = delete;

        bool Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir);
        void Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FLensEffectsConstants& Constants);
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
