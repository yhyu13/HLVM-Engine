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

            // Set 0 — SRV-only (cbuffer + 5 SRVs; t4 = primary ray direction,
            // Phase B).
            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::ConstantBuffer(256),
                nvrhi::BindingLayoutItem::Texture_SRV(0),   // Radiance
                nvrhi::BindingLayoutItem::Texture_SRV(1),   // WorldPos
                nvrhi::BindingLayoutItem::Texture_SRV(2),   // Normal
                nvrhi::BindingLayoutItem::Texture_SRV(3),   // Depth
                nvrhi::BindingLayoutItem::Texture_SRV(4)    // Direction
            };

            GenerationLayoutSRV = Device->createBindingLayout(LayoutDesc);
        }
        {
            nvrhi::BindingLayoutDesc LayoutDesc;
            LayoutDesc.visibility = nvrhi::ShaderType::Compute;

            nvrhi::VulkanBindingOffsets offsets;
            offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
            LayoutDesc.setBindingOffsets(offsets);

            // Set 1 — UAV-only (2 reservoir UAVs at uRegister 384..385).
            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::Texture_UAV(384), // Reservoir0
                nvrhi::BindingLayoutItem::Texture_UAV(385)  // Reservoir1
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
                nvrhi::BindingLayoutItem::Texture_SRV(9)
            };

            TemporalLayoutSRV = Device->createBindingLayout(LayoutDesc);
        }
        {
            nvrhi::BindingLayoutDesc LayoutDesc;
            LayoutDesc.visibility = nvrhi::ShaderType::Compute;

            nvrhi::VulkanBindingOffsets offsets;
            offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
            LayoutDesc.setBindingOffsets(offsets);

            // Set 1 — UAV-only layout (3 UAVs at uRegister 384..386).
            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::Texture_UAV(384),
                nvrhi::BindingLayoutItem::Texture_UAV(385),
                nvrhi::BindingLayoutItem::Texture_UAV(386)
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

            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::ConstantBuffer(256),
                nvrhi::BindingLayoutItem::Texture_SRV(0),   // Radiance
                nvrhi::BindingLayoutItem::Texture_SRV(1),   // Reservoir0
                nvrhi::BindingLayoutItem::Texture_SRV(2),   // Reservoir1
                nvrhi::BindingLayoutItem::Texture_SRV(3),   // Normal
                nvrhi::BindingLayoutItem::Texture_SRV(4),   // Depth
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
            nvrhi::BindingSetItem::Texture_SRV(4, Desc.DirectionTexture ? Desc.DirectionTexture : Desc.RadianceTexture)
        };
        nvrhi::BindingSetHandle SRVBindingSet = Device->createBindingSet(SRVSetDesc, GenerationLayoutSRV);

        nvrhi::BindingSetDesc UAVSetDesc;
        UAVSetDesc.bindings = {
            nvrhi::BindingSetItem::Texture_UAV(384, Desc.OutReservoir0),
            nvrhi::BindingSetItem::Texture_UAV(385, Desc.OutReservoir1)
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
            nvrhi::BindingSetItem::Texture_SRV(2, Desc.HistoryReservoir0),
            nvrhi::BindingSetItem::Texture_SRV(3, Desc.HistoryReservoir1),
            nvrhi::BindingSetItem::Texture_SRV(4, Desc.DepthTexture),
            nvrhi::BindingSetItem::Texture_SRV(5, Desc.NormalTexture),
            nvrhi::BindingSetItem::Texture_SRV(6, Desc.PrevDepthTexture),
            nvrhi::BindingSetItem::Texture_SRV(7, Desc.PrevNormalTexture),
            nvrhi::BindingSetItem::Texture_SRV(8, Desc.CurrentRadiance),
            nvrhi::BindingSetItem::Texture_SRV(9, Desc.HistoryRadiance)
        };
        nvrhi::BindingSetHandle SRVBindingSet = Device->createBindingSet(SRVSetDesc, TemporalLayoutSRV);

        nvrhi::BindingSetDesc UAVSetDesc;
        UAVSetDesc.bindings = {
            nvrhi::BindingSetItem::Texture_UAV(384, Desc.OutReservoir0),
            nvrhi::BindingSetItem::Texture_UAV(385, Desc.OutReservoir1),
            nvrhi::BindingSetItem::Texture_UAV(386, Desc.OutRadiance)
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

        CmdList->writeBuffer(ConstantBuffer, ConstantsData, sizeof(ConstantsData));

        nvrhi::BindingSetDesc BindingSetDesc;
        BindingSetDesc.bindings = {
            nvrhi::BindingSetItem::ConstantBuffer(256, ConstantBuffer),
            nvrhi::BindingSetItem::Texture_SRV(0, Desc.RadianceTexture),
            nvrhi::BindingSetItem::Texture_SRV(1, Desc.Reservoir0),
            nvrhi::BindingSetItem::Texture_SRV(2, Desc.Reservoir1),
            nvrhi::BindingSetItem::Texture_SRV(3, Desc.NormalTexture),
            nvrhi::BindingSetItem::Texture_SRV(4, Desc.DepthTexture),
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
