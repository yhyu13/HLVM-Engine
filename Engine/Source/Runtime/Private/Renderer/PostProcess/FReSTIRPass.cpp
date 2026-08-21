// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/PostProcess/FReSTIRPass.h"
#include "Core/Log.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
#include <glm/glm.hpp>
#include <fstream>

DECLARE_LOG_CATEGORY(LogPostProcess)

// ---------------------------------------------------------------------------
// Helper
// ---------------------------------------------------------------------------

static std::vector<char> ReadBinaryFile(const std::string& Filename)
{
    std::ifstream file(Filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        return {};
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    file.close();

    return buffer;
}

// ---------------------------------------------------------------------------
// FReSTIRPass
// ---------------------------------------------------------------------------

namespace ReSTIR
{
    bool FReSTIRPass::Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir)
    {
        if (bIsInitialized)
        {
            Shutdown();
        }

        HLVM_LOG(LogPostProcess, info, TXT("FReSTIRPass::Initialize"));

        Device = InDevice;
        ShaderDataDir = InShaderDataDir;

        // =====================================================================
        // Load Generation compute shader
        // =====================================================================
        {
            auto ShaderBlob = ReadBinaryFile(
                FPath::Combine(ShaderDataDir, TXT("ReSTIR_Generate_cs.sblob")).string());
            const void* ShaderBinary = nullptr;
            size_t ShaderBinarySize = 0;
            if (!ShaderMake::FindPermutationInBlob(ShaderBlob.data(), ShaderBlob.size(), nullptr, 0, &ShaderBinary, &ShaderBinarySize))
            {
                HLVM_LOG(LogPostProcess, err, TXT("Failed to extract ReSTIR_Generate_cs from blob"));
                return false;
            }

            nvrhi::ShaderDesc CSDesc;
            CSDesc.setShaderType(nvrhi::ShaderType::Compute);
            GenerationShader = Device->createShader(CSDesc, ShaderBinary, ShaderBinarySize);
            if (!GenerationShader)
            {
                HLVM_LOG(LogPostProcess, err, TXT("Failed to create ReSTIR_Generate_cs shader"));
                return false;
            }
        }

        // =====================================================================
        // Load Temporal compute shader
        // =====================================================================
        {
            auto ShaderBlob = ReadBinaryFile(
                FPath::Combine(ShaderDataDir, TXT("ReSTIR_Temporal_cs.sblob")).string());
            const void* ShaderBinary = nullptr;
            size_t ShaderBinarySize = 0;
            if (!ShaderMake::FindPermutationInBlob(ShaderBlob.data(), ShaderBlob.size(), nullptr, 0, &ShaderBinary, &ShaderBinarySize))
            {
                HLVM_LOG(LogPostProcess, err, TXT("Failed to extract ReSTIR_Temporal_cs from blob"));
                return false;
            }

            nvrhi::ShaderDesc CSDesc;
            CSDesc.setShaderType(nvrhi::ShaderType::Compute);
            TemporalShader = Device->createShader(CSDesc, ShaderBinary, ShaderBinarySize);
            if (!TemporalShader)
            {
                HLVM_LOG(LogPostProcess, err, TXT("Failed to create ReSTIR_Temporal_cs shader"));
                return false;
            }
        }

        // =====================================================================
        // Load Spatial compute shader
        // =====================================================================
        {
            auto ShaderBlob = ReadBinaryFile(
                FPath::Combine(ShaderDataDir, TXT("ReSTIR_Spatial_cs.sblob")).string());
            const void* ShaderBinary = nullptr;
            size_t ShaderBinarySize = 0;
            if (!ShaderMake::FindPermutationInBlob(ShaderBlob.data(), ShaderBlob.size(), nullptr, 0, &ShaderBinary, &ShaderBinarySize))
            {
                HLVM_LOG(LogPostProcess, err, TXT("Failed to extract ReSTIR_Spatial_cs from blob"));
                return false;
            }

            nvrhi::ShaderDesc CSDesc;
            CSDesc.setShaderType(nvrhi::ShaderType::Compute);
            SpatialShader = Device->createShader(CSDesc, ShaderBinary, ShaderBinarySize);
            if (!SpatialShader)
            {
                HLVM_LOG(LogPostProcess, err, TXT("Failed to create ReSTIR_Spatial_cs shader"));
                return false;
            }
        }

        // =====================================================================
        // Create generation binding layouts (v151 — split SRV/UAV per the
        // bug-075 pattern that already fixed TemporalLayout).
        //
        // The single combined layout above mixed Texture_SRV (which forces
        // SHADER_READ_ONLY_OPTIMAL) and Texture_UAV (which forces GENERAL)
        // in one descriptor set. nvrhi's setComputeState binds the descriptor
        // set BEFORE the implicit barriers land, so the validation layer
        // sees the SRV-bound image in the WRONG layout (GENERAL — last
        // touched by the GBuffer raster pass) at bind time, and the GPU may
        // read garbage. This is the same pattern that the temporal layout
        // already split (lines 158-198 below); the generate layout was
        // overlooked when the temporal split landed in bug-075. PENDING_COMMIT_v151
        // mirrors the temporal fix onto the generation layout, the only
        // ReSTIR dispatch whose binding layout was still mixed SRV+UAV.
        //
        // Mirror of the temporal split: GenerationLayoutSRV holds the
        // cbuffer + 4 SRVs (set 0); GenerationLayoutUAV holds the 2
        // reservoir UAVs (set 1). The pipeline composes both layouts in
        // declaration order so descriptor sets are recorded in the same
        // order the dispatch binds them.
        // =====================================================================
        {
            nvrhi::BindingLayoutDesc LayoutDesc;
            LayoutDesc.visibility = nvrhi::ShaderType::Compute;

            nvrhi::VulkanBindingOffsets offsets;
            offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
            LayoutDesc.setBindingOffsets(offsets);

            // Set 0 — SRV-only (cbuffer + 7 SRVs; v210 adds t5 SampleInfo +
            // t6 Material for the ZetaRay RIS packaging).
            //
            // v202: SHARED LAYOUT, DIVERGENT CONSUMERS. Texture_SRV(4) is
            // unconditional, but only the TestReSTIR_GI_Temporal_Data copy of
            // ReSTIR_Generate_cs.hlsl declares a t4 (gDirection); the
            // TestCornellBoxGI_Data copy declares t0..t3 only. That target's
            // pipeline is thus built from a layout advertising a binding its
            // SPIR-V lacks — latent only because DispatchGeneration substitutes
            // RadianceTexture for a null DirectionTexture, keeping the
            // descriptor populated.
            //
            // Invisible to every dual-copy check here, which are all *sameness*
            // checks (v182, v187/v188, v200): these two copies are correctly
            // different. The relation to check is layout-vs-each-consumer:
            // every consumer's shader must declare every binding the layout
            // declares.
            //
            // NOT FIXED HERE ON PURPOSE — the divergent copy is in the
            // known-good control, unmodified by design until the v183+ chain's
            // first build. Card M.
            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::ConstantBuffer(256),
                nvrhi::BindingLayoutItem::Texture_SRV(0),   // Radiance
                nvrhi::BindingLayoutItem::Texture_SRV(1),   // WorldPos
                nvrhi::BindingLayoutItem::Texture_SRV(2),   // Normal
                nvrhi::BindingLayoutItem::Texture_SRV(3),   // Depth
                nvrhi::BindingLayoutItem::Texture_SRV(4),   // Direction (x2Pos+ID)
                nvrhi::BindingLayoutItem::Texture_SRV(5),   // SampleInfo (x2Normal+pdf)
                nvrhi::BindingLayoutItem::Texture_SRV(6)    // Material (albedo+roughness)
            };

            GenerationLayoutSRV = Device->createBindingLayout(LayoutDesc);
        }
        {
            nvrhi::BindingLayoutDesc LayoutDesc;
            LayoutDesc.visibility = nvrhi::ShaderType::Compute;

            nvrhi::VulkanBindingOffsets offsets;
            offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
            LayoutDesc.setBindingOffsets(offsets);

            // Set 1 — UAV-only (3 reservoir UAVs at uRegister 384..386; v210
            // adds Reservoir2 = float4(w_sum, W, OctEncode(x2Normal))).
            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::Texture_UAV(384), // Reservoir0
                nvrhi::BindingLayoutItem::Texture_UAV(385), // Reservoir1
                nvrhi::BindingLayoutItem::Texture_UAV(386)  // Reservoir2
            };

            GenerationLayoutUAV = Device->createBindingLayout(LayoutDesc);
        }

        // =====================================================================
        // Create temporal binding layouts (bug-075 split: set 0 SRVs + set 1 UAVs)
        // =====================================================================
        // SPIR-V reflection of ReSTIR_Temporal_cs.hlsl after the space1
        // declaration on the UAVs:
        //   Set 0, Binding 0:       cbuffer
        //   Set 0, Bindings 0..9:   Texture2D (gCurr/HistReservoirs + radiance)
        //   Set 1, Binding 384/385: RWTexture2D (gOutReservoir0/1)
        {
            nvrhi::BindingLayoutDesc LayoutDesc;
            LayoutDesc.visibility = nvrhi::ShaderType::Compute;

            nvrhi::VulkanBindingOffsets offsets;
            offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
            LayoutDesc.setBindingOffsets(offsets);

            // Set 0 — SRV-only layout (cbuffer + 11 SRVs).
            //
            // v203: SHARED LAYOUT vs EACH CONSUMER (the v202 invariant, applied
            // to the layouts v202 did not reach). This list declares t0..t15
            // (v210 adds t10 = CurrentReservoir2, t11 = HistoryReservoir2,
            // t12 = WorldPos, t13 = Material, t14 = PrevWorldPos,
            // t15 = PrevMaterial).
            // The TestReSTIR_GI_Temporal_Data copy of ReSTIR_Temporal_cs.hlsl
            // declares t0..t15 and matches. The TestCornellBoxGI_Data copy
            // declares t0..t7 only — it has no gCurrRadiance/gHistRadiance
            // (0 hits there against 2 in the primary). So that target's
            // pipeline is built from a layout advertising two SRVs its SPIR-V
            // does not contain.
            //
            // Unlike card M's t4, there is NO fallback making this benign:
            // DispatchTemporal binds slots 8 and 9 unconditionally (see the
            // binding set below) with whatever the caller supplied, and
            // TestCornellBoxGI.cpp does supply both. The descriptors are
            // populated; the shader simply has nowhere to receive them. Card N.
            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::ConstantBuffer(256),
                nvrhi::BindingLayoutItem::Texture_SRV(0),
                nvrhi::BindingLayoutItem::Texture_SRV(1),
                nvrhi::BindingLayoutItem::Texture_SRV(2),
                nvrhi::BindingLayoutItem::Texture_SRV(3),
                nvrhi::BindingLayoutItem::Texture_SRV(4),
                nvrhi::BindingLayoutItem::Texture_SRV(5),
                nvrhi::BindingLayoutItem::Texture_SRV(6),
                nvrhi::BindingLayoutItem::Texture_SRV(7),
                nvrhi::BindingLayoutItem::Texture_SRV(8),
                nvrhi::BindingLayoutItem::Texture_SRV(9),
                nvrhi::BindingLayoutItem::Texture_SRV(10),
                nvrhi::BindingLayoutItem::Texture_SRV(11),
                nvrhi::BindingLayoutItem::Texture_SRV(12),
                nvrhi::BindingLayoutItem::Texture_SRV(13),
                nvrhi::BindingLayoutItem::Texture_SRV(14),
                nvrhi::BindingLayoutItem::Texture_SRV(15)
            };

            TemporalLayoutSRV = Device->createBindingLayout(LayoutDesc);
        }
        {
            nvrhi::BindingLayoutDesc LayoutDesc;
            LayoutDesc.visibility = nvrhi::ShaderType::Compute;

            nvrhi::VulkanBindingOffsets offsets;
            offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
            LayoutDesc.setBindingOffsets(offsets);

            // Set 1 — UAV-only layout (4 UAVs at uRegister 384..387; v210
            // adds OutReservoir2).
            //
            // v203: the sharpest instance of the layout-vs-consumer class found
            // so far, because the divergence is not merely in COUNT but in
            // DESCRIPTOR SET MEMBERSHIP. The comment above records the SPIR-V
            // reflection this split was designed against: UAVs in set 1 because
            // the primary's shader declares them `register(uN, space1)`.
            // The TestCornellBoxGI_Data copy declares only
            // `register(u0)`/`register(u1)` — two UAVs, and in the DEFAULT
            // space, so its UAVs reflect into set 0 alongside the SRVs rather
            // than into set 1.
            //
            // The `space1` divergence is intra-pair, not a project convention:
            // that same target's ReSTIR_Generate_cs.hlsl DOES use
            // `register(u0, space1)`, so its temporal copy is the outlier
            // within its own directory. That is the controlled positive which
            // makes this a real finding rather than a house-style difference.
            //
            // NOT FIXED HERE ON PURPOSE — the divergent copy is in the
            // known-good control. See card N and the note on card L.
            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::Texture_UAV(384),
                nvrhi::BindingLayoutItem::Texture_UAV(385),
                nvrhi::BindingLayoutItem::Texture_UAV(386),
                nvrhi::BindingLayoutItem::Texture_UAV(387)
            };

            TemporalLayoutUAV = Device->createBindingLayout(LayoutDesc);
        }

        // =====================================================================
        // Create spatial binding layout
        // =====================================================================
        {
            nvrhi::BindingLayoutDesc LayoutDesc;
            LayoutDesc.visibility = nvrhi::ShaderType::Compute;

            nvrhi::VulkanBindingOffsets offsets;
            offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
            LayoutDesc.setBindingOffsets(offsets);

            // v202: this layout MIXES 5 SRVs and 1 UAV in one set, unlike
            // GenerationLayout*/TemporalLayout* which bug-075/v151 split. The
            // asymmetry is DELIBERATE — recorded so future cycles stop reading
            // it as an oversight and "finish" the split.
            //
            // bug-075 needs a texture reachable as BOTH SRV and UAV in one
            // dispatch; the temporal pass had that (history and output pairs
            // could alias). Spatial cannot: its SRVs are gi_raw, a parity-
            // selected TemporalReservoir pair, GBuffer normal and linear depth,
            // while its lone UAV (SpatialRadiance) sits in no SRV slot. Nothing
            // to fix. Corroborated: the VUID the split prevents
            // (VkDescriptorImageInfo-imageLayout-00344) is in no retained log.
            //
            // Binding the spatial OUTPUT as one of these SRVs would void this
            // reasoning and make the split required.
            // v203: layout-vs-each-consumer — this is the one pair of the six
            // that is CLEAN, and recording a clean verdict is deliberate.
            // Both copies of ReSTIR_Spatial_cs.hlsl declare exactly t0..t7
            // (v210 adds t5 = Reservoir2, t6 = WorldPos, t7 = Material) plus a single
            // `RWTexture2D gOutput : register(u0)`, matching this list
            // binding-for-binding. Note the control's spatial UAV is in the
            // default space and so is the primary's — here that agreement is
            // correct, because this layout is UNSPLIT and expects everything in
            // set 0. The same default-space declaration that is right here is
            // exactly what is wrong in the control's temporal copy above, where
            // the layout IS split. The convention is per-layout, not per-file.
            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::ConstantBuffer(256),
                nvrhi::BindingLayoutItem::Texture_SRV(0),   // Radiance
                nvrhi::BindingLayoutItem::Texture_SRV(1),   // Reservoir0
                nvrhi::BindingLayoutItem::Texture_SRV(2),   // Reservoir1
                nvrhi::BindingLayoutItem::Texture_SRV(3),   // Normal
                nvrhi::BindingLayoutItem::Texture_SRV(4),   // Depth
                nvrhi::BindingLayoutItem::Texture_SRV(5),   // Reservoir2 (v210)
                nvrhi::BindingLayoutItem::Texture_SRV(6),   // WorldPos (v210)
                nvrhi::BindingLayoutItem::Texture_SRV(7),   // Material (v210)
                nvrhi::BindingLayoutItem::Texture_UAV(384)  // Output radiance
            };

            SpatialLayout = Device->createBindingLayout(LayoutDesc);
        }

        // =====================================================================
        // Create compute pipelines
        // =====================================================================
        {
            nvrhi::ComputePipelineDesc PipelineDesc;
            PipelineDesc.setComputeShader(GenerationShader);
            // v151: pipeline composes both split layouts in declaration order
            // (SRV first = set 0, UAV second = set 1). nvrhi records
            // descriptor sets in this order; the binding sets we use in
            // DispatchGeneration must match (SRV set = set 0, UAV set = set 1).
            // The shader's UAVs are at register(u0/u1, space1) (per the
            // temporal shader's space1 declaration that the generate shader
            // mirrors).
            PipelineDesc.addBindingLayout(GenerationLayoutSRV);
            PipelineDesc.addBindingLayout(GenerationLayoutUAV);
            GenerationPipeline = Device->createComputePipeline(PipelineDesc);
            if (!GenerationPipeline)
            {
                HLVM_LOG(LogPostProcess, err, TXT("Failed to create ReSTIR generation pipeline"));
                return false;
            }
        }

        {
            nvrhi::ComputePipelineDesc PipelineDesc;
            PipelineDesc.setComputeShader(TemporalShader);
            // bug-075: pipeline composes both split layouts in declaration
            // order (SRV first = set 0, UAV second = set 1). nvrhi records
            // descriptor sets in this order; the binding sets we use in
            // DispatchTemporal must match (SRV set = set 0, UAV set = set 1).
            PipelineDesc.addBindingLayout(TemporalLayoutSRV);
            PipelineDesc.addBindingLayout(TemporalLayoutUAV);
            TemporalPipeline = Device->createComputePipeline(PipelineDesc);
            if (!TemporalPipeline)
            {
                HLVM_LOG(LogPostProcess, err, TXT("Failed to create ReSTIR temporal pipeline"));
                return false;
            }
        }

        {
            nvrhi::ComputePipelineDesc PipelineDesc;
            PipelineDesc.setComputeShader(SpatialShader);
            PipelineDesc.addBindingLayout(SpatialLayout);
            SpatialPipeline = Device->createComputePipeline(PipelineDesc);
            if (!SpatialPipeline)
            {
                HLVM_LOG(LogPostProcess, err, TXT("Failed to create ReSTIR spatial pipeline"));
                return false;
            }
        }

        // =====================================================================
        // Create constant buffer
        // =====================================================================
        {
            nvrhi::BufferDesc BufferDesc;
            BufferDesc.byteSize = 256;
            BufferDesc.isConstantBuffer = true;
            BufferDesc.isVolatile = false;
            BufferDesc.keepInitialState = true;
            BufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
            BufferDesc.debugName = "ReSTIRConstants";
            ConstantBuffer = Device->createBuffer(BufferDesc);
        }

        bIsInitialized = true;
        HLVM_LOG(LogPostProcess, info, TXT("FReSTIRPass initialized successfully"));
        return true;
    }

    void FReSTIRPass::DispatchGeneration(nvrhi::ICommandList* CmdList, const FGenerationDesc& Desc, const FReSTIRConstants& Constants)
    {
        if (!CmdList || !GenerationPipeline || !ConstantBuffer)
            return;

        uint32_t outputW = Desc.OutputWidth;
        uint32_t outputH = Desc.OutputHeight;
        if (!outputW && Desc.OutReservoir0)
        {
            auto texDesc = Desc.OutReservoir0->getDesc();
            outputW = texDesc.width;
            outputH = texDesc.height;
        }

        if (outputW == 0 || outputH == 0)
        {
            HLVM_LOG(LogPostProcess, warn, TXT("FReSTIRPass::DispatchGeneration: invalid output dimensions"));
            return;
        }

        float ConstantsData[64];
        memset(ConstantsData, 0, sizeof(ConstantsData));

        size_t offset = 0;
        ConstantsData[offset++] = Constants.OutputSize[0];
        ConstantsData[offset++] = Constants.OutputSize[1];
        ConstantsData[offset++] = Constants.RcpOutputSize[0];
        ConstantsData[offset++] = Constants.RcpOutputSize[1];
        ConstantsData[offset++] = Constants.FrameIndex;
        ConstantsData[offset++] = Constants.NumCandidates;
        ConstantsData[offset++] = Constants.DepthThreshold;
        ConstantsData[offset++] = Constants.NormalThreshold;
        ConstantsData[offset++] = Constants.DebugVis;
        ConstantsData[offset++] = Constants.GBufferScale;

        CmdList->writeBuffer(ConstantBuffer, ConstantsData, sizeof(ConstantsData));

        // v151: the GenerationLayout is split into GenerationLayoutSRV
        // (cbuffer + 4 SRVs) and GenerationLayoutUAV (2 UAVs). We bind both
        // sets in a SINGLE dispatch — the generate shader reads the
        // gi_raw / GBuffer inputs and writes the reservoirs, with no
        // ping-pong aliasing on the same physical texture (unlike the
        // temporal pass where CurrentReservoir and HistoryReservoir can
        // alias the same storage). The split is the same nvrhi-deferred-
        // barrier-ordering mitigation already applied to TemporalLayout.
        nvrhi::BindingSetDesc SRVSetDesc;
        SRVSetDesc.bindings = {
            nvrhi::BindingSetItem::ConstantBuffer(256, ConstantBuffer),
            nvrhi::BindingSetItem::Texture_SRV(0, Desc.RadianceTexture),
            nvrhi::BindingSetItem::Texture_SRV(1, Desc.WorldPosTexture),
            nvrhi::BindingSetItem::Texture_SRV(2, Desc.NormalTexture),
            nvrhi::BindingSetItem::Texture_SRV(3, Desc.DepthTexture),
            nvrhi::BindingSetItem::Texture_SRV(4, Desc.DirectionTexture ? Desc.DirectionTexture : Desc.RadianceTexture),
            nvrhi::BindingSetItem::Texture_SRV(5, Desc.SampleInfoTexture ? Desc.SampleInfoTexture : Desc.RadianceTexture),
            nvrhi::BindingSetItem::Texture_SRV(6, Desc.MaterialTexture ? Desc.MaterialTexture : Desc.RadianceTexture)
        };
        // v202: the ternary is load-bearing for TestCornellBoxGI, which never
        // sets DirectionTexture — it keeps t4's descriptor populated so the
        // shared layout (see the v202 note at GenerationLayoutSRV) binds no
        // null handle. It does not reconcile layout with shader. Card M.
        //
        // v202: generation needs NO GBufferScale, structurally. Temporal and
        // spatial got one at v183 because they sample full-res GBuffer guides
        // from a half-res dispatch; this shader samples no GBuffer texture at
        // all (gWorldPos/gNormals/gDepth are declared in both copies, Loaded in
        // neither — main touches only gRadiance and gDirection). Both live
        // inputs are half-res like the dispatch, so a scale would have nothing
        // to correct. Do not add one by analogy with the sibling passes.
        nvrhi::BindingSetHandle SRVBindingSet = Device->createBindingSet(SRVSetDesc, GenerationLayoutSRV);

        nvrhi::BindingSetDesc UAVSetDesc;
        UAVSetDesc.bindings = {
            nvrhi::BindingSetItem::Texture_UAV(384, Desc.OutReservoir0),
            nvrhi::BindingSetItem::Texture_UAV(385, Desc.OutReservoir1),
            nvrhi::BindingSetItem::Texture_UAV(386, Desc.OutReservoir2)
        };
        nvrhi::BindingSetHandle UAVBindingSet = Device->createBindingSet(UAVSetDesc, GenerationLayoutUAV);

        uint32_t dispatchX = (outputW + 7) / 8;
        uint32_t dispatchY = (outputH + 7) / 8;

        nvrhi::ComputeState ComputeState;
        ComputeState.setPipeline(GenerationPipeline);
        ComputeState.addBindingSet(SRVBindingSet);
        ComputeState.addBindingSet(UAVBindingSet);
        CmdList->setComputeState(ComputeState);
        CmdList->dispatch(dispatchX, dispatchY, 1);
    }

    void FReSTIRPass::DispatchTemporal(nvrhi::ICommandList* CmdList, const FTemporalDesc& Desc, const FReSTIRTemporalConstants& Constants)
    {
        if (!CmdList || !TemporalPipeline || !ConstantBuffer)
            return;

        uint32_t outputW = Desc.OutputWidth;
        uint32_t outputH = Desc.OutputHeight;
        if (!outputW && Desc.OutReservoir0)
        {
            auto texDesc = Desc.OutReservoir0->getDesc();
            outputW = texDesc.width;
            outputH = texDesc.height;
        }

        if (outputW == 0 || outputH == 0)
        {
            HLVM_LOG(LogPostProcess, warn, TXT("FReSTIRPass::DispatchTemporal: invalid output dimensions"));
            return;
        }

        float ConstantsData[64];
        memset(ConstantsData, 0, sizeof(ConstantsData));

        size_t offset = 0;
        memcpy(&ConstantsData[offset], Constants.InverseCurrViewProj, 64);
        offset += 16;
        memcpy(&ConstantsData[offset], Constants.PrevViewProj, 64);
        offset += 16;
        ConstantsData[offset++] = Constants.OutputSize[0];
        ConstantsData[offset++] = Constants.OutputSize[1];
        ConstantsData[offset++] = Constants.RcpOutputSize[0];
        ConstantsData[offset++] = Constants.RcpOutputSize[1];
        ConstantsData[offset++] = Constants.FrameIndex;
        ConstantsData[offset++] = Constants.MaxM;
        ConstantsData[offset++] = Constants.DepthThreshold;
        ConstantsData[offset++] = Constants.NormalThreshold;
        ConstantsData[offset++] = Constants.DebugVis;
        ConstantsData[offset++] = Constants.SceneYaw;
        ConstantsData[offset++] = Constants.PrevSceneYaw;
        // v183: marshalling previously stopped here, so Pad[0]/Pad[1] never
        // reached the GPU — the shader read near=far=0 at :140-141 and its
        // ndcZ reconstruction silently degenerated. GBufferScale (the half-res
        // -> full-res GBuffer ratio) sits in the next slot and needs the same
        // treatment. This struct is marshalled field-by-field, NOT memcpy'd:
        // any field added to FReSTIRTemporalConstants must be appended here.
        // v184: these three are plain scalars on BOTH sides now. They must
        // never become an array again — HLSL forces each constant-buffer array
        // element onto a fresh 16-byte register, so `float Pad[2]` here read
        // back at floats 44/48 instead of 43/44 and pushed GBufferScale to 52,
        // leaving it zero (and the v183 half-res fix inert via max(s,1)).
        ConstantsData[offset++] = Constants.NearPlane;       // near plane
        ConstantsData[offset++] = Constants.FarPlane;        // far plane
        ConstantsData[offset++] = Constants.GBufferScale;

        CmdList->writeBuffer(ConstantBuffer, ConstantsData, sizeof(ConstantsData));

        // The temporal shader declares its UAVs at register(u0/u1/u2, space1)
        // → SPIR-V set 1, while the SRVs/cbuffer live in set 0. The binding
        // layout is split into TemporalLayoutSRV (set 0) + TemporalLayoutUAV
        // (set 1) and the pipeline composes both. We bind both sets in a
        // SINGLE dispatch: the shader reads the history SRVs before writing
        // the output UAVs, which is safe even where the ping-pong aliases a
        // history texture with an output texture (per-thread read-before-write).
        nvrhi::BindingSetDesc SRVSetDesc;
        SRVSetDesc.bindings = {
            nvrhi::BindingSetItem::ConstantBuffer(256, ConstantBuffer),
            nvrhi::BindingSetItem::Texture_SRV(0, Desc.CurrentReservoir0),
            nvrhi::BindingSetItem::Texture_SRV(1, Desc.CurrentReservoir1),
            nvrhi::BindingSetItem::Texture_SRV(10, Desc.CurrentReservoir2),
            nvrhi::BindingSetItem::Texture_SRV(2, Desc.HistoryReservoir0),
            nvrhi::BindingSetItem::Texture_SRV(3, Desc.HistoryReservoir1),
            nvrhi::BindingSetItem::Texture_SRV(11, Desc.HistoryReservoir2),
            nvrhi::BindingSetItem::Texture_SRV(4, Desc.DepthTexture),
            nvrhi::BindingSetItem::Texture_SRV(5, Desc.NormalTexture),
            nvrhi::BindingSetItem::Texture_SRV(6, Desc.PrevDepthTexture),
            nvrhi::BindingSetItem::Texture_SRV(7, Desc.PrevNormalTexture),
            nvrhi::BindingSetItem::Texture_SRV(8, Desc.CurrentRadiance),
            nvrhi::BindingSetItem::Texture_SRV(9, Desc.HistoryRadiance),
            nvrhi::BindingSetItem::Texture_SRV(12, Desc.WorldPosTexture ? Desc.WorldPosTexture : Desc.NormalTexture),
            nvrhi::BindingSetItem::Texture_SRV(13, Desc.MaterialTexture ? Desc.MaterialTexture : Desc.NormalTexture),
            nvrhi::BindingSetItem::Texture_SRV(14, Desc.PrevWorldPosTexture ? Desc.PrevWorldPosTexture : Desc.PrevNormalTexture),
            nvrhi::BindingSetItem::Texture_SRV(15, Desc.PrevMaterialTexture ? Desc.PrevMaterialTexture : Desc.PrevNormalTexture)
        };
        nvrhi::BindingSetHandle SRVBindingSet = Device->createBindingSet(SRVSetDesc, TemporalLayoutSRV);

        nvrhi::BindingSetDesc UAVSetDesc;
        UAVSetDesc.bindings = {
            nvrhi::BindingSetItem::Texture_UAV(384, Desc.OutReservoir0),
            nvrhi::BindingSetItem::Texture_UAV(385, Desc.OutReservoir1),
            nvrhi::BindingSetItem::Texture_UAV(386, Desc.OutReservoir2),
            nvrhi::BindingSetItem::Texture_UAV(387, Desc.OutRadiance)
        };
        nvrhi::BindingSetHandle UAVBindingSet = Device->createBindingSet(UAVSetDesc, TemporalLayoutUAV);

        uint32_t dispatchX = (outputW + 7) / 8;
        uint32_t dispatchY = (outputH + 7) / 8;

        // Single dispatch. The old bug-075 workaround ran the shader TWICE
        // ("SRV-only phase" + "UAV-only phase"), but both phases actually
        // bound the same two sets, so dispatch 2 re-read the history textures
        // AFTER dispatch 1 had overwritten them (the ping-pong aliases
        // Hist0/Out1 and Hist1/Out0 on the same physical texture). The second
        // pass then merged the freshly-written (M,W,0,0) as if it were history
        // radiance — producing the all-yellow (1,1,0) spatial output. One
        // dispatch reads the history before writing it, which is correct.
        {
            nvrhi::ComputeState ComputeState;
            ComputeState.setPipeline(TemporalPipeline);
            ComputeState.addBindingSet(SRVBindingSet);
            ComputeState.addBindingSet(UAVBindingSet);
            CmdList->setComputeState(ComputeState);
            CmdList->dispatch(dispatchX, dispatchY, 1);
        }
    }

    void FReSTIRPass::DispatchSpatial(nvrhi::ICommandList* CmdList, const FSpatialDesc& Desc, const FReSTIRSpatialConstants& Constants)
    {
        if (!CmdList || !SpatialPipeline || !ConstantBuffer)
            return;

        uint32_t outputW = Desc.OutputWidth;
        uint32_t outputH = Desc.OutputHeight;
        if (!outputW && Desc.OutRadiance)
        {
            auto texDesc = Desc.OutRadiance->getDesc();
            outputW = texDesc.width;
            outputH = texDesc.height;
        }

        if (outputW == 0 || outputH == 0)
        {
            HLVM_LOG(LogPostProcess, warn, TXT("FReSTIRPass::DispatchSpatial: invalid output dimensions"));
            return;
        }

        float ConstantsData[64];
        memset(ConstantsData, 0, sizeof(ConstantsData));

        size_t offset = 0;
        ConstantsData[offset++] = Constants.OutputSize[0];
        ConstantsData[offset++] = Constants.OutputSize[1];
        ConstantsData[offset++] = Constants.RcpOutputSize[0];
        ConstantsData[offset++] = Constants.RcpOutputSize[1];
        ConstantsData[offset++] = Constants.NormalThreshold;
        ConstantsData[offset++] = Constants.DepthThreshold;
        ConstantsData[offset++] = Constants.MaxM;
        ConstantsData[offset++] = Constants.SpatialRadius;
        ConstantsData[offset++] = Constants.DebugVis;
        // v183: see the note in DispatchTemporal — field-by-field marshalling,
        // so GBufferScale must be appended explicitly or it stays zero.
        ConstantsData[offset++] = Constants.GBufferScale;

        CmdList->writeBuffer(ConstantBuffer, ConstantsData, sizeof(ConstantsData));

        nvrhi::BindingSetDesc BindingSetDesc;
        BindingSetDesc.bindings = {
            nvrhi::BindingSetItem::ConstantBuffer(256, ConstantBuffer),
            nvrhi::BindingSetItem::Texture_SRV(0, Desc.RadianceTexture),
            nvrhi::BindingSetItem::Texture_SRV(1, Desc.Reservoir0),
            nvrhi::BindingSetItem::Texture_SRV(2, Desc.Reservoir1),
            nvrhi::BindingSetItem::Texture_SRV(5, Desc.Reservoir2),
            nvrhi::BindingSetItem::Texture_SRV(3, Desc.NormalTexture),
            nvrhi::BindingSetItem::Texture_SRV(4, Desc.DepthTexture),
            nvrhi::BindingSetItem::Texture_SRV(6, Desc.WorldPosTexture ? Desc.WorldPosTexture : Desc.NormalTexture),
            nvrhi::BindingSetItem::Texture_SRV(7, Desc.MaterialTexture ? Desc.MaterialTexture : Desc.NormalTexture),
            nvrhi::BindingSetItem::Texture_UAV(384, Desc.OutRadiance)
        };
        nvrhi::BindingSetHandle BindingSet = Device->createBindingSet(BindingSetDesc, SpatialLayout);

        uint32_t dispatchX = (outputW + 7) / 8;
        uint32_t dispatchY = (outputH + 7) / 8;

        nvrhi::ComputeState ComputeState;
        ComputeState.setPipeline(SpatialPipeline);
        ComputeState.addBindingSet(BindingSet);
        CmdList->setComputeState(ComputeState);
        CmdList->dispatch(dispatchX, dispatchY, 1);
    }

    void FReSTIRPass::Shutdown()
    {
        HLVM_LOG(LogPostProcess, info, TXT("FReSTIRPass::Shutdown"));

        SpatialPipeline = nullptr;
        TemporalPipeline = nullptr;
        GenerationPipeline = nullptr;
        SpatialLayout = nullptr;
        TemporalLayoutSRV = nullptr;
        TemporalLayoutUAV = nullptr;
        GenerationLayoutSRV = nullptr; // v151: split per bug-075 pattern
        GenerationLayoutUAV = nullptr; // v151: split per bug-075 pattern
        SpatialShader = nullptr;
        TemporalShader = nullptr;
        GenerationShader = nullptr;
        ConstantBuffer = nullptr;
        Device = nullptr;
        bIsInitialized = false;
    }
} // namespace ReSTIR
