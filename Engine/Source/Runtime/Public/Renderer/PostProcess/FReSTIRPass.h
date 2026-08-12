// Copyright 2026 HLVM Engine
//
// MIT License
//
// FReSTIRPass — ReSTIR GI Reservoir Pass (Corrected Implementation)
//
// Screen-space ReSTIR with:
//   - Generation: M candidates via tent distribution, streaming RIS
//   - Temporal: reprojection + prev-frame depth/normal validation
//   - Spatial: 3x3 merge with geometric rejection, outputs selected radiance

#pragma once

#include "Core/String.h"
#include <nvrhi/nvrhi.h>

namespace ReSTIR
{
    struct FReSTIRConstants
    {
        TFP32 OutputSize[2];
        TFP32 RcpOutputSize[2];
        TFP32 FrameIndex;
        TFP32 NumCandidates;
        TFP32 DepthThreshold;
        TFP32 NormalThreshold;
        TFP32 DebugVis;
        TFP32 Pad[2];
    };

    struct FReSTIRTemporalConstants
    {
        TFP32 InverseCurrViewProj[16];
        TFP32 PrevViewProj[16];
        TFP32 OutputSize[2];
        TFP32 RcpOutputSize[2];
        TFP32 FrameIndex;
        TFP32 MaxM;
        TFP32 DepthThreshold;
        TFP32 NormalThreshold;
        TFP32 DebugVis;
        TFP32 SceneYaw;      // Phase C: scene Y-rotation this frame (deg)
        TFP32 PrevSceneYaw;  // Phase C: scene Y-rotation previous frame (deg)
        TFP32 Pad[3];
    };

    struct FReSTIRSpatialConstants
    {
        TFP32 OutputSize[2];
        TFP32 RcpOutputSize[2];
        TFP32 NormalThreshold;
        TFP32 DepthThreshold;
        TFP32 MaxM;
        TFP32 SpatialRadius;
        TFP32 DebugVis;
        TFP32 Pad[2];
    };

    class FReSTIRPass
    {
    public:
        struct FGenerationDesc
        {
            nvrhi::TextureHandle RadianceTexture;
            nvrhi::TextureHandle DirectionTexture;   // t4: primary ray direction (u2 from FGIPass)
            nvrhi::TextureHandle WorldPosTexture;
            nvrhi::TextureHandle NormalTexture;
            nvrhi::TextureHandle DepthTexture;
            nvrhi::TextureHandle OutReservoir0;
            nvrhi::TextureHandle OutReservoir1;
            uint32_t OutputWidth = 0;
            uint32_t OutputHeight = 0;
        };

        struct FTemporalDesc
        {
            nvrhi::TextureHandle CurrentReservoir0;
            nvrhi::TextureHandle CurrentReservoir1;
            nvrhi::TextureHandle HistoryReservoir0;
            nvrhi::TextureHandle HistoryReservoir1;
            nvrhi::TextureHandle CurrentRadiance;
            nvrhi::TextureHandle HistoryRadiance;
            nvrhi::TextureHandle DepthTexture;
            nvrhi::TextureHandle NormalTexture;
            nvrhi::TextureHandle PrevDepthTexture;
            nvrhi::TextureHandle PrevNormalTexture;
            nvrhi::TextureHandle OutReservoir0;
            nvrhi::TextureHandle OutReservoir1;
            nvrhi::TextureHandle OutRadiance;
            uint32_t OutputWidth = 0;
            uint32_t OutputHeight = 0;
        };

        struct FSpatialDesc
        {
            nvrhi::TextureHandle RadianceTexture;
            nvrhi::TextureHandle Reservoir0;
            nvrhi::TextureHandle Reservoir1;
            nvrhi::TextureHandle NormalTexture;
            nvrhi::TextureHandle DepthTexture;
            nvrhi::TextureHandle OutRadiance;
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
        void DispatchGeneration(nvrhi::ICommandList* CmdList, const FGenerationDesc& Desc, const FReSTIRConstants& Constants);
        void DispatchTemporal(nvrhi::ICommandList* CmdList, const FTemporalDesc& Desc, const FReSTIRTemporalConstants& Constants);
        void DispatchSpatial(nvrhi::ICommandList* CmdList, const FSpatialDesc& Desc, const FReSTIRSpatialConstants& Constants);
        void Shutdown();

    private:
        nvrhi::IDevice* Device = nullptr;
        nvrhi::ShaderHandle GenerationShader;
        nvrhi::ShaderHandle TemporalShader;
        nvrhi::ShaderHandle SpatialShader;
        // v151 (six-role-pipeline): the generation shader's binding layout
        // is split into SRV-only (set 0) + UAV-only (set 1) per the bug-075
        // pattern that already fixed TemporalLayout. The mixed single-layout
        // field that lived here (GenerationLayout) was removed — there is
        // no ABI requirement on private fields, and keeping an unassigned
        // member invites future code paths to construct a broken binding
        // set against a stale handle.
        nvrhi::BindingLayoutHandle GenerationLayoutSRV;
        nvrhi::BindingLayoutHandle GenerationLayoutUAV;
        // bug-075 (six-role-pipeline v1): the temporal shader declares UAVs
        // at register(u0, space1) / register(u1, space1) → SPIR-V set 1.
        // The binding layout is split into TemporalLayoutSRV (set 0: SRVs +
        // cbuffer) and TemporalLayoutUAV (set 1: UAVs). The pipeline
        // composes both in declaration order. Each dispatch binds ONE set
        // (SRV-only or UAV-only), so nvrhi's requireTextureState infers
        // only the matching state per dispatch — eliminating the SRV/UAV
        // descriptor-set collision that produced
        // VUID-VkDescriptorImageInfo-imageLayout-00344.
        nvrhi::BindingLayoutHandle TemporalLayoutSRV;
        nvrhi::BindingLayoutHandle TemporalLayoutUAV;
        nvrhi::BindingLayoutHandle SpatialLayout;
        nvrhi::ComputePipelineHandle GenerationPipeline;
        nvrhi::ComputePipelineHandle TemporalPipeline;
        nvrhi::ComputePipelineHandle SpatialPipeline;
        nvrhi::BufferHandle ConstantBuffer;
        FString ShaderDataDir;
        bool bIsInitialized = false;
    };
} // namespace ReSTIR
