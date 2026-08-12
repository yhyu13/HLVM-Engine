// Copyright 2026 HLVM Engine
//
// MIT License

#pragma once

#include "Core/String.h"
#include "Utility/CVar/CVarMacros.h"
#include <nvrhi/nvrhi.h>

namespace ReBLUR
{
    // Denoiser tuning CVars (2026-08-11): the FPooledBlurParams defaults are
    // CVar-backed (r_ReBLUR_*), per the "expose denoiser blend factors as
    // CVars" rule — previously hardcoded struct defaults.
    HLVM_INLINE_VAR float g_ReBLURBlurRadius = 8.0f;
    AUTO_CVAR_REF_FLOAT(r_ReBLUR_BlurRadius, g_ReBLURBlurRadius, "ReBLUR spatial blur radius (px)", EConsoleVariableFlag::Saved)
    HLVM_INLINE_VAR float g_ReBLURNormalWeight = 2.0f;
    AUTO_CVAR_REF_FLOAT(r_ReBLUR_NormalWeight, g_ReBLURNormalWeight, "ReBLUR normal rejection weight", EConsoleVariableFlag::Saved)
    HLVM_INLINE_VAR float g_ReBLURPlaneWeight = 12.0f;
    AUTO_CVAR_REF_FLOAT(r_ReBLUR_PlaneWeight, g_ReBLURPlaneWeight, "ReBLUR relative-depth rejection weight", EConsoleVariableFlag::Saved)
    HLVM_INLINE_VAR float g_ReBLURRoughnessWeight = 0.3f;
    AUTO_CVAR_REF_FLOAT(r_ReBLUR_RoughnessWeight, g_ReBLURRoughnessWeight, "ReBLUR roughness rejection weight", EConsoleVariableFlag::Saved)
    HLVM_INLINE_VAR float g_ReBLURAntiLagIntensity = 0.5f;
    AUTO_CVAR_REF_FLOAT(r_ReBLUR_AntiLagIntensity, g_ReBLURAntiLagIntensity, "ReBLUR anti-lag intensity", EConsoleVariableFlag::Saved)
    HLVM_INLINE_VAR float g_ReBLURDarknessSensitivity = 0.01f;
    AUTO_CVAR_REF_FLOAT(r_ReBLUR_DarknessSensitivity, g_ReBLURDarknessSensitivity, "ReBLUR darkness sensitivity", EConsoleVariableFlag::Saved)
    HLVM_INLINE_VAR float g_ReBLURSpatialAlpha = 1.0f;
    AUTO_CVAR_REF_FLOAT(r_ReBLUR_SpatialAlpha, g_ReBLURSpatialAlpha, "ReBLUR spatial blend alpha", EConsoleVariableFlag::Saved)

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
        TFP32 SpatialAlpha;              // Spatial blend alpha (0=temporal only, 1=full spatial)
        TFP32 PassIndex;                 // 0 = temporal, 1 = spatial
        TFP32 Pad[2];
    };
    static_assert(sizeof(FReBLURConstants) == 340, "FReBLURConstants must be 340 bytes");

    struct FPooledBlurParams
    {
        // 2026-08-09: tuned so the spatial blur actually denoises.
        // PlaneWeight now scales a LINEAR-DEPTH bilateral term (|d_n - d_c|),
        // so 40 rejects depth deltas > ~0.05 while accepting coplanar pixels.
        float BlurRadius = 8.0f;
        float NormalWeight = 2.0f;
        float PlaneWeight = 12.0f;
        float RoughnessWeight = 0.3f;
        float AntiLagIntensity = 0.5f;
        float DarknessSensitivity = 0.01f;
        float SpatialAlpha = 1.0f;
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

        // Smoke-test overload: spatial-only blur with internally-created dummy depth/normal.
        // Uses Input as both current and history (no temporal accumulation).
        void Dispatch(nvrhi::ICommandList* CmdList, nvrhi::TextureHandle Input, nvrhi::TextureHandle Output, uint32_t W, uint32_t H);

        void Shutdown();

        // Helpers to set default blur params
        static FPooledBlurParams GetDefaultBlurParams()
        {
            FPooledBlurParams P;
            P.BlurRadius = g_ReBLURBlurRadius;
            P.NormalWeight = g_ReBLURNormalWeight;
            P.PlaneWeight = g_ReBLURPlaneWeight;
            P.RoughnessWeight = g_ReBLURRoughnessWeight;
            P.AntiLagIntensity = g_ReBLURAntiLagIntensity;
            P.DarknessSensitivity = g_ReBLURDarknessSensitivity;
            P.SpatialAlpha = g_ReBLURSpatialAlpha;
            return P;
        }

    private:
        nvrhi::IDevice* Device = nullptr;
        nvrhi::ShaderHandle ComputeShader;
        nvrhi::BindingLayoutHandle BindingLayout;
        nvrhi::ComputePipelineHandle Pipeline;
        nvrhi::BufferHandle ConstantBuffer;
        nvrhi::SamplerHandle PointSampler;
        nvrhi::SamplerHandle LinearSampler;
        nvrhi::TextureHandle DummyDepthTexture;
        nvrhi::TextureHandle DummyNormalTexture;
        FString ShaderDataDir;
        bool bIsInitialized = false;
    };
} // namespace ReBLUR
