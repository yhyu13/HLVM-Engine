#pragma once

#include "Core/String.h"
#include <nvrhi/nvrhi.h>

namespace DOF
{
    struct FDOFConstants
    {
        TFP32 FocalDepth;
        TFP32 Aperture;
        TFP32 DepthScale;
        TFP32 MaxBlurRadius;
        TFP32 OutputSize[2];
        TFP32 RcpOutputSize[2];
        TFP32 Pad[4];
    };
    static_assert(sizeof(FDOFConstants) == 48, "FDOFConstants must be 48 bytes");

    class FDOFPass
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

        FDOFPass() = default;
        ~FDOFPass() { Shutdown(); }

        FDOFPass(const FDOFPass&) = delete;
        FDOFPass& operator=(const FDOFPass&) = delete;
        FDOFPass(FDOFPass&&) = delete;
        FDOFPass& operator=(FDOFPass&&) = delete;

        bool Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir);
        void Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FDOFConstants& Constants);
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
