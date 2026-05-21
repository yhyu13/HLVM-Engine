// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/PostProcess/FTAAPass.h"
#include "Core/Log.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
#include <fstream>
#include <vector>

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
// FTAAPass
// ---------------------------------------------------------------------------

namespace TAA
{
    bool FTAAPass::Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir)
    {
        if (bIsInitialized)
        {
            Shutdown();
        }

        HLVM_LOG(LogPostProcess, info, TXT("FTAAPass::Initialize"));

        Device = InDevice;
        ShaderDataDir = InShaderDataDir;

        // Load compute shader from .sblob
        auto ShaderBlob = ReadBinaryFile(
            FPath::Combine(InShaderDataDir, TXT("TAA_cs.sblob")).string());
        const void* ShaderBinary = nullptr;
        size_t ShaderBinarySize = 0;
        if (!ShaderMake::FindPermutationInBlob(ShaderBlob.data(), ShaderBlob.size(), nullptr, 0, &ShaderBinary, &ShaderBinarySize))
        {
            HLVM_LOG(LogPostProcess, err, TXT("Failed to extract TAA_cs from blob"));
            return false;
        }

        nvrhi::ShaderDesc CSDesc;
        CSDesc.setShaderType(nvrhi::ShaderType::Compute);
        ComputeShader = Device->createShader(CSDesc, ShaderBinary, ShaderBinarySize);
        if (!ComputeShader)
        {
            HLVM_LOG(LogPostProcess, err, TXT("Failed to create TAA_cs shader"));
            return false;
        }

        // Create linear sampler
        {
            nvrhi::SamplerDesc LinearDesc;
            LinearDesc.setAddressU(nvrhi::SamplerAddressMode::ClampToEdge)
                      .setAddressV(nvrhi::SamplerAddressMode::ClampToEdge)
                      .setAddressW(nvrhi::SamplerAddressMode::ClampToEdge)
                      .setMinFilter(true)
                      .setMagFilter(true)
                      .setMipFilter(true);
            LinearSampler = Device->createSampler(LinearDesc);
        }

        // Create binding layout
        {
            nvrhi::BindingLayoutDesc LayoutDesc;
            LayoutDesc.visibility = nvrhi::ShaderType::Compute;

            nvrhi::VulkanBindingOffsets offsets;
            offsets.setConstantBufferOffset(0)
                   .setShaderResourceOffset(0)
                   .setSamplerOffset(0)
                   .setUnorderedAccessViewOffset(0);
            LayoutDesc.setBindingOffsets(offsets);

            // b0 -> 256 (TAAConstants)
            // t0 -> 0   (Current frame)
            // t1 -> 1   (History frame)
            // t2 -> 2   (Depth texture)
            // s0 -> 128 (Linear clamp sampler)
            // u0 -> 384 (TAA output)
            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::ConstantBuffer(256),
                nvrhi::BindingLayoutItem::Texture_SRV(0),
                nvrhi::BindingLayoutItem::Texture_SRV(1),
                nvrhi::BindingLayoutItem::Texture_SRV(2),
                nvrhi::BindingLayoutItem::Sampler(128),
                nvrhi::BindingLayoutItem::Texture_UAV(384)
            };

            BindingLayout = Device->createBindingLayout(LayoutDesc);
        }

        // Create compute pipeline
        {
            nvrhi::ComputePipelineDesc PipelineDesc;
            PipelineDesc.setComputeShader(ComputeShader);
            PipelineDesc.addBindingLayout(BindingLayout);

            Pipeline = Device->createComputePipeline(PipelineDesc);
            if (!Pipeline)
            {
                HLVM_LOG(LogPostProcess, err, TXT("Failed to create TAA compute pipeline"));
                return false;
            }
        }

        // Create constant buffer
        {
            nvrhi::BufferDesc BufferDesc;
            BufferDesc.byteSize = sizeof(FTAAConstants);
            BufferDesc.isConstantBuffer = true;
            BufferDesc.isVolatile = false;
            BufferDesc.keepInitialState = true;
            BufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
            BufferDesc.debugName = "TAAConstants";
            ConstantBuffer = Device->createBuffer(BufferDesc);
        }

        HLVM_LOG(LogPostProcess, info, TXT("FTAAPass initialized successfully"));
        bIsInitialized = true;
        return true;
    }

    void FTAAPass::Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FTAAConstants& Constants)
    {
        if (!bIsInitialized || !CmdList || !Pipeline || !ConstantBuffer)
            return;

        uint32_t outputW = Desc.OutputWidth;
        uint32_t outputH = Desc.OutputHeight;
        if (!outputW && Desc.OutputTexture)
        {
            auto texDesc = Desc.OutputTexture->getDesc();
            outputW = texDesc.width;
            outputH = texDesc.height;
        }

        if (outputW == 0 || outputH == 0)
        {
            HLVM_LOG(LogPostProcess, warn, TXT("FTAAPass::Dispatch: invalid output dimensions"));
            return;
        }

        // Upload constants
        float ConstantsData[40]; // 160 bytes
        memset(ConstantsData, 0, sizeof(ConstantsData));

        // InverseCurrViewProj (16 floats)
        memcpy(&ConstantsData[0], Constants.InverseCurrViewProj, 64);
        // PrevViewProj (16 floats)
        memcpy(&ConstantsData[16], Constants.PrevViewProj, 64);
        // OutputSize
        ConstantsData[32] = Constants.OutputSize[0];
        ConstantsData[33] = Constants.OutputSize[1];
        // RcpOutputSize
        ConstantsData[34] = Constants.RcpOutputSize[0];
        ConstantsData[35] = Constants.RcpOutputSize[1];
        // BlendFactor
        ConstantsData[36] = Constants.BlendFactor;
        // DepthThreshold
        ConstantsData[37] = Constants.DepthThreshold;

        CmdList->writeBuffer(ConstantBuffer, ConstantsData, sizeof(ConstantsData));

        // Create binding set
        nvrhi::BindingSetDesc BindingSetDesc;
        BindingSetDesc.bindings = {
            nvrhi::BindingSetItem::ConstantBuffer(256, ConstantBuffer),
            nvrhi::BindingSetItem::Texture_SRV(0, Desc.CurrentFrameTexture),
            nvrhi::BindingSetItem::Texture_SRV(1, Desc.HistoryFrameTexture),
            nvrhi::BindingSetItem::Texture_SRV(2, Desc.DepthTexture),
            nvrhi::BindingSetItem::Sampler(128, LinearSampler),
            nvrhi::BindingSetItem::Texture_UAV(384, Desc.OutputTexture)
        };
        nvrhi::BindingSetHandle BindingSet = Device->createBindingSet(BindingSetDesc, BindingLayout);

        // Dispatch
        uint32_t dispatchX = (outputW + 7) / 8;
        uint32_t dispatchY = (outputH + 7) / 8;

        nvrhi::ComputeState ComputeState;
        ComputeState.setPipeline(Pipeline);
        ComputeState.addBindingSet(BindingSet);
        CmdList->setComputeState(ComputeState);
        CmdList->dispatch(dispatchX, dispatchY, 1);
    }

    void FTAAPass::Shutdown()
    {
        HLVM_LOG(LogPostProcess, info, TXT("FTAAPass::Shutdown"));

        Pipeline = nullptr;
        BindingLayout = nullptr;
        ComputeShader = nullptr;
        ConstantBuffer = nullptr;
        LinearSampler = nullptr;
        Device = nullptr;
        bIsInitialized = false;
    }
}
