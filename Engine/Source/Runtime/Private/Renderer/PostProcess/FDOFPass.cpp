// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/PostProcess/FDOFPass.h"
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
// FDOFPass
// ---------------------------------------------------------------------------

namespace DOF
{
    bool FDOFPass::Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir)
    {
        if (bIsInitialized)
        {
            Shutdown();
        }

        HLVM_LOG(LogPostProcess, info, TXT("FDOFPass::Initialize"));

        Device = InDevice;
        ShaderDataDir = InShaderDataDir;

        // Load compute shader from .sblob
        auto ShaderBlob = ReadBinaryFile(
            FPath::Combine(InShaderDataDir, TXT("DOF_cs.sblob")).string());
        const void* ShaderBinary = nullptr;
        size_t ShaderBinarySize = 0;
        if (!ShaderMake::FindPermutationInBlob(ShaderBlob.data(), ShaderBlob.size(), nullptr, 0, &ShaderBinary, &ShaderBinarySize))
        {
            HLVM_LOG(LogPostProcess, err, TXT("Failed to extract DOF_cs from blob"));
            return false;
        }

        nvrhi::ShaderDesc CSDesc;
        CSDesc.setShaderType(nvrhi::ShaderType::Compute);
        ComputeShader = Device->createShader(CSDesc, ShaderBinary, ShaderBinarySize);
        if (!ComputeShader)
        {
            HLVM_LOG(LogPostProcess, err, TXT("Failed to create DOF_cs shader"));
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

            // b0 -> 256 (DOFConstants)
            // t0 -> 0   (Color texture)
            // t1 -> 1   (Depth texture)
            // s0 -> 128 (Linear clamp sampler)
            // u0 -> 384 (DOF output)
            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::ConstantBuffer(256),
                nvrhi::BindingLayoutItem::Texture_SRV(0),
                nvrhi::BindingLayoutItem::Texture_SRV(1),
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
                HLVM_LOG(LogPostProcess, err, TXT("Failed to create DOF compute pipeline"));
                return false;
            }
        }

        // Create constant buffer
        {
            nvrhi::BufferDesc BufferDesc;
            BufferDesc.byteSize = sizeof(FDOFConstants);
            BufferDesc.isConstantBuffer = true;
            BufferDesc.isVolatile = false;
            BufferDesc.keepInitialState = true;
            BufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
            BufferDesc.debugName = "DOFConstants";
            ConstantBuffer = Device->createBuffer(BufferDesc);
        }

        HLVM_LOG(LogPostProcess, info, TXT("FDOFPass initialized successfully"));
        bIsInitialized = true;
        return true;
    }

    void FDOFPass::Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FDOFConstants& Constants)
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
            HLVM_LOG(LogPostProcess, warn, TXT("FDOFPass::Dispatch: invalid output dimensions"));
            return;
        }

        // Upload constants
        float ConstantsData[12]; // 48 bytes
        memset(ConstantsData, 0, sizeof(ConstantsData));

        ConstantsData[0] = Constants.FocalDepth;
        ConstantsData[1] = Constants.Aperture;
        ConstantsData[2] = Constants.DepthScale;
        ConstantsData[3] = Constants.MaxBlurRadius;
        ConstantsData[4] = Constants.OutputSize[0];
        ConstantsData[5] = Constants.OutputSize[1];
        ConstantsData[6] = Constants.RcpOutputSize[0];
        ConstantsData[7] = Constants.RcpOutputSize[1];

        CmdList->writeBuffer(ConstantBuffer, ConstantsData, sizeof(ConstantsData));

        // Create binding set
        nvrhi::BindingSetDesc BindingSetDesc;
        BindingSetDesc.bindings = {
            nvrhi::BindingSetItem::ConstantBuffer(256, ConstantBuffer),
            nvrhi::BindingSetItem::Texture_SRV(0, Desc.ColorTexture),
            nvrhi::BindingSetItem::Texture_SRV(1, Desc.DepthTexture),
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

    void FDOFPass::Shutdown()
    {
        HLVM_LOG(LogPostProcess, info, TXT("FDOFPass::Shutdown"));

        Pipeline = nullptr;
        BindingLayout = nullptr;
        ComputeShader = nullptr;
        ConstantBuffer = nullptr;
        LinearSampler = nullptr;
        Device = nullptr;
        bIsInitialized = false;
    }
}
