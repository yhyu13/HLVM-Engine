// Copyright 2026 HLVM Engine
//
// MIT License
//
// FReSTIRPass — ReSTIR GI Reservoir Pass (Phase 7a + 7b)
//
// Phase 7a: Generation — converts GI output into per-pixel reservoirs
// Phase 7b: Temporal  — merges reservoirs with reprojected history

#pragma once

#include "Core/String.h"
#include <nvrhi/nvrhi.h>

namespace ReSTIR
{
    // Phase 7a constants
    struct FReSTIRConstants
    {
        TFP32 OutputSize[2];
        TFP32 RcpOutputSize[2];
        TFP32 FrameIndex;
        TFP32 DebugVis;
        TFP32 Pad[2];
    };

    // Phase 7b constants
    struct FReSTIRTemporalConstants
    {
        TFP32 InverseCurrViewProj[16];
        TFP32 PrevViewProj[16];
        TFP32 OutputSize[2];
        TFP32 RcpOutputSize[2];
        TFP32 FrameIndex;
        TFP32 MaxM;
        TFP32 DebugVis;
        TFP32 Pad;
    };

    // Phase 8 constants
    struct FReSTIRSpatialConstants
    {
        TFP32 OutputSize[2];
        TFP32 RcpOutputSize[2];
        TFP32 NormalSigma;
        TFP32 PlaneSigma;
        TFP32 DepthSigma;
        TFP32 MaxM;
        TFP32 SpatialRadius;
        TFP32 DebugVis;
    };

    class FReSTIRPass
    {
    public:
        // Phase 7a descriptor
        struct FDesc
        {
            nvrhi::TextureHandle RadianceTexture;
            nvrhi::TextureHandle WorldPosTexture;
            nvrhi::TextureHandle NormalTexture;
            nvrhi::TextureHandle DepthTexture;
            nvrhi::TextureHandle OutReservoir0;
            nvrhi::TextureHandle OutReservoir1;
            nvrhi::TextureHandle OutDebugTexture;
            uint32_t OutputWidth = 0;
            uint32_t OutputHeight = 0;
        };

        // Phase 7b descriptor
        struct FTemporalDesc
        {
            nvrhi::TextureHandle CurrentReservoir0;
            nvrhi::TextureHandle CurrentReservoir1;
            nvrhi::TextureHandle HistoryReservoir0;
            nvrhi::TextureHandle HistoryReservoir1;
            nvrhi::TextureHandle DepthTexture;
            nvrhi::TextureHandle OutReservoir0;
            nvrhi::TextureHandle OutReservoir1;
            nvrhi::TextureHandle OutDebugTexture;
            uint32_t OutputWidth = 0;
            uint32_t OutputHeight = 0;
        };

        // Phase 8 descriptor
        struct FSpatialDesc
        {
            nvrhi::TextureHandle MergedReservoir0;
            nvrhi::TextureHandle MergedReservoir1;
            nvrhi::TextureHandle NormalTexture;
            nvrhi::TextureHandle DepthTexture;
            nvrhi::TextureHandle RadianceTexture;
            nvrhi::TextureHandle OutRadiance;
            nvrhi::TextureHandle OutDebugTexture;
            uint32_t OutputWidth = 0;
            uint32_t OutputHeight = 0;
        };

        FReSTIRPass() = default;
        ~FReSTIRPass() { Shutdown(); }

        FReSTIRPass(const FReSTIRPass&) = delete;
        FReSTIRPass& operator=(const FReSTIRPass&) = delete;
        FReSTIRPass(FReSTIRPass&&) = delete;
        FReSTIRPass& operator=(FReSTIRPass&&) = delete;

        bool Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir);
        void Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FReSTIRConstants& Constants);
        void DispatchTemporal(nvrhi::ICommandList* CmdList, const FTemporalDesc& Desc, const FReSTIRTemporalConstants& Constants);
        void DispatchSpatial(nvrhi::ICommandList* CmdList, const FSpatialDesc& Desc, const FReSTIRSpatialConstants& Constants);
        void Shutdown();

    private:
        nvrhi::IDevice* Device = nullptr;
        nvrhi::ShaderHandle GenerationShader;
        nvrhi::ShaderHandle TemporalShader;
        nvrhi::ShaderHandle SpatialShader;
        nvrhi::BindingLayoutHandle GenerationLayout;
        nvrhi::BindingLayoutHandle TemporalLayout;
        nvrhi::BindingLayoutHandle SpatialLayout;
        nvrhi::ComputePipelineHandle GenerationPipeline;
        nvrhi::ComputePipelineHandle TemporalPipeline;
        nvrhi::ComputePipelineHandle SpatialPipeline;
        nvrhi::BufferHandle ConstantBuffer;
        FString ShaderDataDir;
        bool bIsInitialized = false;
    };
} // namespace ReSTIR
