// Copyright 2026 HLVM Engine
//
// MIT License

#pragma once

#include "Core/String.h"
#include <nvrhi/nvrhi.h>

namespace ReBLUR
{
    struct FReBLURConstants
    {
        TFP32 InverseCurrViewProj[16];  // Current frame's inverse ViewProj
        TFP32 PrevViewProj[16];          // Previous frame's ViewProj
        TFP32 ViewMatrix[16];           // Current view matrix
        TFP32 ProjMatrix[16];           // Current projection matrix
        TFP32 OutputSize[2];            // Output dimensions
        TFP32 RcpOutputSize[2];          // 1 / Output dimensions
        TFP32 HitDistParams[4];         // A, B, C, D for hit distance normalization
        TFP32 BlurRadius;               // Spatial blur radius in pixels
        TFP32 NormalWeight;             // Normal rejection weight
        TFP32 PlaneWeight;              // Plane distance weight
        TFP32 RoughnessWeight;          // Roughness rejection weight
        TFP32 AntiLagIntensity;          // Anti-lag feedback intensity (0-1)
        TFP32 DarknessSensitivity;       // Darkness sensitivity for anti-lag
        TFP32 FrameIndex;               // Frame counter
        TFP32 HistoryFadeIn;            // Frames to fade in history
        TFP32 ConfidenceScale;           // Confidence scale factor
        TFP32 PassIndex;                 // 0 = temporal, 1 = spatial
        TFP32 Pad[2];
    };
    static_assert(sizeof(FReBLURConstants) == 336, "FReBLURConstants must be 336 bytes");

    struct FPooledBlurParams
    {
        float BlurRadius = 12.0f;
        float NormalWeight = 0.1f;
        float PlaneWeight = 100.0f;
        float RoughnessWeight = 0.3f;
        float AntiLagIntensity = 0.5f;
        float DarknessSensitivity = 0.01f;
    };

    class FReBLURPass
    {
    public:
        struct FDesc
        {
            nvrhi::TextureHandle CurrentRadianceTexture;   // Noisy GI radiance (RGB) + hit distance (A)
            nvrhi::TextureHandle HistoryTexture;           // Ping-pong history buffer (RGBA16F)
            nvrhi::TextureHandle DepthTexture;             // Linear depth
            nvrhi::TextureHandle NormalRoughnessTexture;  // Normal (RGB) + roughness (A)
            nvrhi::TextureHandle OutputTexture;            // Denoised output
            uint32_t OutputWidth = 0;
            uint32_t OutputHeight = 0;
        };

        FReBLURPass() = default;
        ~FReBLURPass() { Shutdown(); }

        FReBLURPass(const FReBLURPass&) = delete;
        FReBLURPass& operator=(const FReBLURPass&) = delete;
        FReBLURPass(FReBLURPass&&) = delete;
        FReBLURPass& operator=(FReBLURPass&&) = delete;

        bool Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir);
        void Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FReBLURConstants& Constants, const FPooledBlurParams& BlurParams);
        void Shutdown();

        // Helpers to set default blur params
        static FPooledBlurParams GetDefaultBlurParams() { return FPooledBlurParams{}; }

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
} // namespace ReBLUR
