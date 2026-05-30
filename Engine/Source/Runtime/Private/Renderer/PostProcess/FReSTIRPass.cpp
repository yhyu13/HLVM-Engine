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
        // Load Generation compute shader from .sblob
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
        // Load Temporal compute shader from .sblob
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
        // Load Spatial compute shader from .sblob
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
        // Create generation binding layout
        // =====================================================================
        {
            nvrhi::BindingLayoutDesc LayoutDesc;
            LayoutDesc.visibility = nvrhi::ShaderType::Compute;

            nvrhi::VulkanBindingOffsets offsets;
            offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
            LayoutDesc.setBindingOffsets(offsets);

            // b0 -> 256 (Constants)
            // t0 -> 0 (Radiance texture)
            // t1 -> 1 (WorldPos texture)
            // t2 -> 2 (Normal texture)
            // t3 -> 3 (Depth texture)
            // u0 -> 384 (Reservoir0)
            // u1 -> 385 (Reservoir1)
            // u2 -> 386 (Debug output)
            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::ConstantBuffer(256),
                nvrhi::BindingLayoutItem::Texture_SRV(0),   // Radiance
                nvrhi::BindingLayoutItem::Texture_SRV(1),   // WorldPos
                nvrhi::BindingLayoutItem::Texture_SRV(2),   // Normal
                nvrhi::BindingLayoutItem::Texture_SRV(3),   // Depth
                nvrhi::BindingLayoutItem::Texture_UAV(384), // Reservoir0
                nvrhi::BindingLayoutItem::Texture_UAV(385), // Reservoir1
                nvrhi::BindingLayoutItem::Texture_UAV(386)  // Debug output
            };

            GenerationLayout = Device->createBindingLayout(LayoutDesc);
        }

        // =====================================================================
        // Create temporal binding layout
        // =====================================================================
        {
            nvrhi::BindingLayoutDesc LayoutDesc;
            LayoutDesc.visibility = nvrhi::ShaderType::Compute;

            nvrhi::VulkanBindingOffsets offsets;
            offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
            LayoutDesc.setBindingOffsets(offsets);

            // b0 -> 256 (Constants)
            // t0 -> 0 (Current Reservoir0)
            // t1 -> 1 (Current Reservoir1)
            // t2 -> 2 (History Reservoir0)
            // t3 -> 3 (History Reservoir1)
            // t4 -> 4 (Depth)
            // u0 -> 384 (Merged Reservoir0)
            // u1 -> 385 (Merged Reservoir1)
            // u2 -> 386 (Debug output)
            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::ConstantBuffer(256),
                nvrhi::BindingLayoutItem::Texture_SRV(0),   // Current R0
                nvrhi::BindingLayoutItem::Texture_SRV(1),   // Current R1
                nvrhi::BindingLayoutItem::Texture_SRV(2),   // History R0
                nvrhi::BindingLayoutItem::Texture_SRV(3),   // History R1
                nvrhi::BindingLayoutItem::Texture_SRV(4),   // Depth
                nvrhi::BindingLayoutItem::Texture_UAV(384), // Merged R0
                nvrhi::BindingLayoutItem::Texture_UAV(385), // Merged R1
                nvrhi::BindingLayoutItem::Texture_UAV(386)  // Debug output
            };

            TemporalLayout = Device->createBindingLayout(LayoutDesc);
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

            // b0 -> 256 (Constants)
            // t0 -> 0 (Merged Reservoir0)
            // t1 -> 1 (Merged Reservoir1)
            // t2 -> 2 (Normal texture)
            // t3 -> 3 (Depth texture)
            // t4 -> 4 (Radiance texture)
            // u0 -> 384 (Output radiance)
            // u1 -> 385 (Debug output)
            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::ConstantBuffer(256),
                nvrhi::BindingLayoutItem::Texture_SRV(0),   // Merged R0
                nvrhi::BindingLayoutItem::Texture_SRV(1),   // Merged R1
                nvrhi::BindingLayoutItem::Texture_SRV(2),   // Normal
                nvrhi::BindingLayoutItem::Texture_SRV(3),   // Depth
                nvrhi::BindingLayoutItem::Texture_SRV(4),   // Radiance
                nvrhi::BindingLayoutItem::Texture_UAV(384), // Output radiance
                nvrhi::BindingLayoutItem::Texture_UAV(385)  // Debug output
            };

            SpatialLayout = Device->createBindingLayout(LayoutDesc);
        }

        // =====================================================================
        // Create compute pipelines
        // =====================================================================
        {
            nvrhi::ComputePipelineDesc PipelineDesc;
            PipelineDesc.setComputeShader(GenerationShader);
            PipelineDesc.addBindingLayout(GenerationLayout);
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
            PipelineDesc.addBindingLayout(TemporalLayout);
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
        // Create constant buffer (256 bytes for alignment)
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

    void FReSTIRPass::Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FReSTIRConstants& Constants)
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
            HLVM_LOG(LogPostProcess, warn, TXT("FReSTIRPass::Dispatch: invalid output dimensions"));
            return;
        }

        // Build constants data (256 bytes)
        float ConstantsData[64];
        memset(ConstantsData, 0, sizeof(ConstantsData));

        size_t offset = 0;
        ConstantsData[offset++] = Constants.OutputSize[0];
        ConstantsData[offset++] = Constants.OutputSize[1];
        ConstantsData[offset++] = Constants.RcpOutputSize[0];
        ConstantsData[offset++] = Constants.RcpOutputSize[1];
        ConstantsData[offset++] = Constants.FrameIndex;
        ConstantsData[offset++] = Constants.DebugVis;

        CmdList->writeBuffer(ConstantBuffer, ConstantsData, sizeof(ConstantsData));

        // Create binding set
        nvrhi::BindingSetDesc BindingSetDesc;
        BindingSetDesc.bindings = {
            nvrhi::BindingSetItem::ConstantBuffer(256, ConstantBuffer),
            nvrhi::BindingSetItem::Texture_SRV(0, Desc.RadianceTexture),
            nvrhi::BindingSetItem::Texture_SRV(1, Desc.WorldPosTexture),
            nvrhi::BindingSetItem::Texture_SRV(2, Desc.NormalTexture),
            nvrhi::BindingSetItem::Texture_SRV(3, Desc.DepthTexture),
            nvrhi::BindingSetItem::Texture_UAV(384, Desc.OutReservoir0),
            nvrhi::BindingSetItem::Texture_UAV(385, Desc.OutReservoir1),
            nvrhi::BindingSetItem::Texture_UAV(386, Desc.OutDebugTexture)
        };
        nvrhi::BindingSetHandle BindingSet = Device->createBindingSet(BindingSetDesc, GenerationLayout);

        // Dispatch (8x8 thread groups)
        uint32_t dispatchX = (outputW + 7) / 8;
        uint32_t dispatchY = (outputH + 7) / 8;

        nvrhi::ComputeState ComputeState;
        ComputeState.setPipeline(GenerationPipeline);
        ComputeState.addBindingSet(BindingSet);
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

        // Build constants data (256 bytes)
        float ConstantsData[64];
        memset(ConstantsData, 0, sizeof(ConstantsData));

        size_t offset = 0;
        // InverseCurrViewProj (16 floats)
        memcpy(&ConstantsData[offset], Constants.InverseCurrViewProj, 64);
        offset += 16;

        // PrevViewProj (16 floats)
        memcpy(&ConstantsData[offset], Constants.PrevViewProj, 64);
        offset += 16;

        ConstantsData[offset++] = Constants.OutputSize[0];
        ConstantsData[offset++] = Constants.OutputSize[1];
        ConstantsData[offset++] = Constants.RcpOutputSize[0];
        ConstantsData[offset++] = Constants.RcpOutputSize[1];
        ConstantsData[offset++] = Constants.FrameIndex;
        ConstantsData[offset++] = Constants.MaxM;
        ConstantsData[offset++] = Constants.DebugVis;
        ConstantsData[offset++] = Constants.Pad;

        CmdList->writeBuffer(ConstantBuffer, ConstantsData, sizeof(ConstantsData));

        // Create binding set
        nvrhi::BindingSetDesc BindingSetDesc;
        BindingSetDesc.bindings = {
            nvrhi::BindingSetItem::ConstantBuffer(256, ConstantBuffer),
            nvrhi::BindingSetItem::Texture_SRV(0, Desc.CurrentReservoir0),
            nvrhi::BindingSetItem::Texture_SRV(1, Desc.CurrentReservoir1),
            nvrhi::BindingSetItem::Texture_SRV(2, Desc.HistoryReservoir0),
            nvrhi::BindingSetItem::Texture_SRV(3, Desc.HistoryReservoir1),
            nvrhi::BindingSetItem::Texture_SRV(4, Desc.DepthTexture),
            nvrhi::BindingSetItem::Texture_UAV(384, Desc.OutReservoir0),
            nvrhi::BindingSetItem::Texture_UAV(385, Desc.OutReservoir1),
            nvrhi::BindingSetItem::Texture_UAV(386, Desc.OutDebugTexture)
        };
        nvrhi::BindingSetHandle BindingSet = Device->createBindingSet(BindingSetDesc, TemporalLayout);

        // Dispatch (8x8 thread groups)
        uint32_t dispatchX = (outputW + 7) / 8;
        uint32_t dispatchY = (outputH + 7) / 8;

        nvrhi::ComputeState ComputeState;
        ComputeState.setPipeline(TemporalPipeline);
        ComputeState.addBindingSet(BindingSet);
        CmdList->setComputeState(ComputeState);
        CmdList->dispatch(dispatchX, dispatchY, 1);
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

        // Build constants data (256 bytes)
        float ConstantsData[64];
        memset(ConstantsData, 0, sizeof(ConstantsData));

        size_t offset = 0;
        ConstantsData[offset++] = Constants.OutputSize[0];
        ConstantsData[offset++] = Constants.OutputSize[1];
        ConstantsData[offset++] = Constants.RcpOutputSize[0];
        ConstantsData[offset++] = Constants.RcpOutputSize[1];
        ConstantsData[offset++] = Constants.NormalSigma;
        ConstantsData[offset++] = Constants.PlaneSigma;
        ConstantsData[offset++] = Constants.DepthSigma;
        ConstantsData[offset++] = Constants.MaxM;
        ConstantsData[offset++] = Constants.SpatialRadius;
        ConstantsData[offset++] = Constants.DebugVis;

        CmdList->writeBuffer(ConstantBuffer, ConstantsData, sizeof(ConstantsData));

        // Create binding set
        nvrhi::BindingSetDesc BindingSetDesc;
        BindingSetDesc.bindings = {
            nvrhi::BindingSetItem::ConstantBuffer(256, ConstantBuffer),
            nvrhi::BindingSetItem::Texture_SRV(0, Desc.MergedReservoir0),
            nvrhi::BindingSetItem::Texture_SRV(1, Desc.MergedReservoir1),
            nvrhi::BindingSetItem::Texture_SRV(2, Desc.NormalTexture),
            nvrhi::BindingSetItem::Texture_SRV(3, Desc.DepthTexture),
            nvrhi::BindingSetItem::Texture_SRV(4, Desc.RadianceTexture),
            nvrhi::BindingSetItem::Texture_UAV(384, Desc.OutRadiance),
            nvrhi::BindingSetItem::Texture_UAV(385, Desc.OutDebugTexture)
        };
        nvrhi::BindingSetHandle BindingSet = Device->createBindingSet(BindingSetDesc, SpatialLayout);

        // Dispatch (8x8 thread groups)
        uint32_t dispatchX = (outputW + 7) / 8;
        uint32_t dispatchY = (outputH + 7) / 8;

        HLVM_LOG(LogPostProcess, info, TXT("DispatchSpatial: dispatching {}x{}x1 to {}x{} texture"), dispatchX, dispatchY, outputW, outputH);
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
        TemporalLayout = nullptr;
        GenerationLayout = nullptr;
        SpatialShader = nullptr;
        TemporalShader = nullptr;
        GenerationShader = nullptr;
        ConstantBuffer = nullptr;
        Device = nullptr;
        bIsInitialized = false;
    }
} // namespace ReSTIR
