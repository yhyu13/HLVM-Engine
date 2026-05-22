#pragma once

#include "Core/String.h"
#include <nvrhi/nvrhi.h>

namespace ContactShadows
{
    struct FContactShadowConstants
    {
        float ViewProj[16];
        float InvViewProj[16];
        float LightDir[3];
        float MaxDistance;
        float ScreenSize[2];
        float RcpScreenSize[2];
        int StepCount;
        float Thickness;
        float Pad[2];
        float Pad2[20];
    };
    static_assert(sizeof(FContactShadowConstants) == 256, "FContactShadowConstants must be 256 bytes");

    class FContactShadowsPass
    {
    public:
        struct FDesc
        {
            nvrhi::TextureHandle DepthTexture;
            nvrhi::TextureHandle OutputTexture;
            uint32_t Width = 0;
            uint32_t Height = 0;
        };

        FContactShadowsPass() = default;
        ~FContactShadowsPass() { Shutdown(); }

        FContactShadowsPass(const FContactShadowsPass&) = delete;
        FContactShadowsPass& operator=(const FContactShadowsPass&) = delete;
        FContactShadowsPass(FContactShadowsPass&&) = delete;
        FContactShadowsPass& operator=(FContactShadowsPass&&) = delete;

        bool Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir);
        void Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FContactShadowConstants& Constants);
        void Shutdown();

    private:
        nvrhi::IDevice* Device = nullptr;
        nvrhi::ShaderHandle ComputeShader;
        nvrhi::BindingLayoutHandle BindingLayout;
        nvrhi::ComputePipelineHandle Pipeline;
        nvrhi::BufferHandle ConstantBuffer;
        nvrhi::SamplerHandle PointSampler;
        FString ShaderDataDir;
        bool bIsInitialized = false;
    };
}
