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
        // v210 (ZetaRay ground-truth port): full-res GBuffer width / half-res
        // dispatch width — the generate pass now reads the full-res GBuffer
        // (material/normal/worldpos) to evaluate f and the RIS target.
        TFP32 GBufferScale;
        // v186: plain scalars, NOT an array, and NOT a float2 on the HLSL
        // side either. These two are currently write-never (the marshaller in
        // FReSTIRPass.cpp stops at DebugVis) and read-never (the generation
        // shader never names them), so this changes no behaviour today. The
        // point is that the C++ and HLSL declarations disagreed in KIND, and
        // the two kinds pack differently the moment a field is appended after
        // them — the trap v184 fell into on the temporal struct. Mirror of
        // ReSTIR_Generate_cs.hlsl — keep the order and the kind identical.
        TFP32 Pad0;
        TFP32 Pad1;
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
        // v184: plain scalars, NOT an array. HLSL puts each constant-buffer
        // array element on its own 16-byte register, so a `Pad[2]` here
        // desynced these three from the flat float offsets FReSTIRPass.cpp
        // writes. Mirror of ReSTIR_Temporal_cs.hlsl — keep the order identical.
        TFP32 NearPlane;     // was Pad[0]
        TFP32 FarPlane;      // was Pad[1]
        TFP32 GBufferScale;  // v183: full-res GBuffer width / half-res dispatch width
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
        TFP32 GBufferScale;  // v183: full-res GBuffer width / half-res dispatch width
        TFP32 Pad;
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
            // v210 (ZetaRay ground-truth port): t5 = SampleInfo (x2Normal + pdf),
            // t6 = GBufferMaterial (albedo for the f term).
            nvrhi::TextureHandle SampleInfoTexture;
            nvrhi::TextureHandle MaterialTexture;
            nvrhi::TextureHandle OutReservoir0;
            nvrhi::TextureHandle OutReservoir1;
            // v210 (ZetaRay ground-truth port, Phase 1): reservoir C —
            // float4(w_sum, W, OctEncode(x2Normal)). A/B hold pos+ID / Lo+M.
            nvrhi::TextureHandle OutReservoir2;
            uint32_t OutputWidth = 0;
            uint32_t OutputHeight = 0;
        };

        struct FTemporalDesc
        {
            nvrhi::TextureHandle CurrentReservoir0;
            nvrhi::TextureHandle CurrentReservoir1;
            nvrhi::TextureHandle CurrentReservoir2;
            nvrhi::TextureHandle HistoryReservoir0;
            nvrhi::TextureHandle HistoryReservoir1;
            nvrhi::TextureHandle HistoryReservoir2;
            // v210: t12/t13 — full-res primary surface (world pos + albedo)
            // for target/BSDF evaluation.
            nvrhi::TextureHandle WorldPosTexture;
            nvrhi::TextureHandle MaterialTexture;
            nvrhi::TextureHandle CurrentRadiance;
            nvrhi::TextureHandle HistoryRadiance;
            // GUIDE EXTENT CONTRACT (v210) — this class is the THIRD of three
            // siblings and holds a contract different from BOTH of the others.
            // Read this before carrying an invariant across from either.
            //   FBilateralDenoisePass: guides free; the CALLEE derives the
            //       ratio from Desc.DepthTexture->getDesc().
            //   FReBLURPass:           guides MUST equal the dispatch extent;
            //       it indexes them raw and has no scale field at all.
            //   FReSTIRPass (here):    guides free, but the CALLER must
            //       compute and supply the ratio as
            //       FReSTIRTemporalConstants::GBufferScale.
            // The four guides below may be full-res GBuffer MRTs while the
            // dispatch is half-res; the shader's GB() helper scales the
            // dispatch coord by GBufferScale before every guide Load. This
            // class CANNOT derive that ratio for you: it calls getDesc() only
            // on OutReservoir0 (the dispatch grid) and never on a guide, and
            // one scale serves all four guides, so there is no single guide it
            // could correctly source it from. Leaving GBufferScale unset is
            // silent — max(int(s),1) in GB() turns 0 into the identity map,
            // which is the v183 defect restored with no VUID and no error.
            // All four guides must therefore share one extent.
            nvrhi::TextureHandle DepthTexture;
            nvrhi::TextureHandle NormalTexture;
            nvrhi::TextureHandle PrevDepthTexture;
            nvrhi::TextureHandle PrevNormalTexture;
            // v210: true previous-frame surface (world pos + material) for
            // candidate validation under the scene turntable.
            nvrhi::TextureHandle PrevWorldPosTexture;
            nvrhi::TextureHandle PrevMaterialTexture;
            // v211 (Phase 4): TLAS for segment-visibility tests in reuse.
            nvrhi::rt::AccelStructHandle SceneTLAS;
            nvrhi::TextureHandle OutReservoir0;
            nvrhi::TextureHandle OutReservoir1;
            nvrhi::TextureHandle OutReservoir2;
            nvrhi::TextureHandle OutRadiance;
            uint32_t OutputWidth = 0;
            uint32_t OutputHeight = 0;
        };

        struct FSpatialDesc
        {
            nvrhi::TextureHandle RadianceTexture;
            nvrhi::TextureHandle Reservoir0;
            nvrhi::TextureHandle Reservoir1;
            nvrhi::TextureHandle Reservoir2;
            // v210: t6/t7 — full-res primary surface (world pos + albedo).
            nvrhi::TextureHandle WorldPosTexture;
            nvrhi::TextureHandle MaterialTexture;
            // v211 (Phase 4): TLAS for segment-visibility tests in reuse.
            nvrhi::rt::AccelStructHandle SceneTLAS;
            // Same contract as FTemporalDesc above — the caller supplies the
            // guide/dispatch ratio as FReSTIRSpatialConstants::GBufferScale.
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
        // v211 (Phase 4): 1x1 fallbacks so TestCornellBoxGI / TestPathTraceGI
        // keep populating the shared v210/v211 layout slots without owning the
        // new textures. Their shaders do not declare/read the extra slots, so
        // the descriptors are inert. The TLAS slot is the exception: it must
        // be a REAL acceleration structure (checked_cast), so callers wire
        // their TLAS explicitly.
        nvrhi::TextureHandle DummyReservoir;   // 1x1 RGBA32F
        nvrhi::TextureHandle DummyGuide;       // 1x1 normal/pos/material stand-in
        FString ShaderDataDir;
        bool bIsInitialized = false;
    };
} // namespace ReSTIR
