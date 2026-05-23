#pragma once

#include "Core/String.h"
#include <nvrhi/nvrhi.h>

class FDeferredLightingPass
{
public:
    struct FDesc
    {
        nvrhi::TextureHandle GBufferDiffuse;
        nvrhi::TextureHandle GBufferMaterial;
        nvrhi::TextureHandle GBufferNormals;
        nvrhi::TextureHandle GBufferEmissive;
        nvrhi::TextureHandle GBufferDepth;
        nvrhi::TextureHandle ShadowMap;
        nvrhi::TextureHandle SSAOTexture;
        nvrhi::TextureHandle ContactShadowTexture;
        nvrhi::TextureHandle HDROutputTexture;
        nvrhi::SamplerHandle ShadowSampler;
        uint32_t Width = 0;
        uint32_t Height = 0;
        bool bEnableSSAO = true;
        bool bEnableContactShadows = true;
    };

    // Shader cbuffer is 13 registers (208 bytes). Padded to 256 for alignment consistency.
    struct FConstants
    {
        float InvViewProj[16];
        float LightDir[3];
        float LightIntensity;
        float CameraPos[3];
        float ShadowHardness;
        float AmbientColor[3];
        float MinAO;
        float ScreenSize[2];
        float Pad1[2];
        float LightViewProj[16];
        float ShadowMapSize;
        float ShadowBias;
        float Pad2[2];
        float Pad3[12];
    };
    static_assert(sizeof(FConstants) == 256, "FDeferredLightingPass constants must be 256 bytes");

    FDeferredLightingPass() = default;
    ~FDeferredLightingPass() { Shutdown(); }

    FDeferredLightingPass(const FDeferredLightingPass&) = delete;
    FDeferredLightingPass& operator=(const FDeferredLightingPass&) = delete;
    FDeferredLightingPass(FDeferredLightingPass&&) = delete;
    FDeferredLightingPass& operator=(FDeferredLightingPass&&) = delete;

    bool Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir);
    void Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FConstants& Constants);
    void Shutdown();

private:
    static constexpr uint32_t PERMUTATION_COUNT = 4;

    nvrhi::IDevice* Device = nullptr;
    nvrhi::BindingLayoutHandle BindingLayout;
    nvrhi::ComputePipelineHandle Pipelines[PERMUTATION_COUNT];
    nvrhi::BufferHandle ConstantBuffer;
    FString ShaderDataDir;
    bool bIsInitialized = false;

    uint32_t GetPermutationIndex(bool bSSAO, bool bContactShadows) const
    {
        return (bSSAO ? 1u : 0u) | (bContactShadows ? 2u : 0u);
    }
};
