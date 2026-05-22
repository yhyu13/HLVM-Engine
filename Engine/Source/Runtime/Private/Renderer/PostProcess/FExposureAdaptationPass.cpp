// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/PostProcess/FExposureAdaptationPass.h"
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
// FExposureAdaptationPass
// ---------------------------------------------------------------------------

namespace Exposure
{
    bool FExposureAdaptationPass::Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir)
    {
        if (bIsInitialized)
        {
            Shutdown();
        }

        HLVM_LOG(LogPostProcess, info, TXT("FExposureAdaptationPass::Initialize"));

        Device = InDevice;
        ShaderDataDir = InShaderDataDir;

        // Load compute shader from .sblob
        auto ShaderBlob = ReadBinaryFile(
            FPath::Combine(InShaderDataDir, TXT("ExposureAdaptation_cs.sblob")).string());
        const void* ShaderBinary = nullptr;
        size_t ShaderBinarySize = 0;
        if (!ShaderMake::FindPermutationInBlob(ShaderBlob.data(), ShaderBlob.size(), nullptr, 0, &ShaderBinary, &ShaderBinarySize))
        {
            HLVM_LOG(LogPostProcess, err, TXT("Failed to extract ExposureAdaptation_cs from blob"));
            return false;
        }

        nvrhi::ShaderDesc CSDesc;
        CSDesc.setShaderType(nvrhi::ShaderType::Compute);
        ComputeShader = Device->createShader(CSDesc, ShaderBinary, ShaderBinarySize);
        if (!ComputeShader)
        {
            HLVM_LOG(LogPostProcess, err, TXT("Failed to create ExposureAdaptation_cs shader"));
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

            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::ConstantBuffer(256),
                nvrhi::BindingLayoutItem::Texture_SRV(0),
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
                HLVM_LOG(LogPostProcess, err, TXT("Failed to create exposure adaptation compute pipeline"));
                return false;
            }
        }

        // Create constant buffer
        {
            nvrhi::BufferDesc BufferDesc;
            BufferDesc.byteSize = sizeof(FExposureConstants);
            BufferDesc.isConstantBuffer = true;
            BufferDesc.isVolatile = false;
            BufferDesc.keepInitialState = true;
            BufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
            BufferDesc.debugName = "ExposureAdaptationConstants";
            ConstantBuffer = Device->createBuffer(BufferDesc);
        }

        HLVM_LOG(LogPostProcess, info, TXT("FExposureAdaptationPass initialized successfully"));
        bIsInitialized = true;
        return true;
    }

    void FExposureAdaptationPass::Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FExposureConstants& Constants)
    {
        if (!bIsInitialized || !CmdList || !Pipeline || !ConstantBuffer)
            return;

        if (!Desc.SceneColorTexture || !Desc.AdaptedLuminanceTexture)
        {
            HLVM_LOG(LogPostProcess, warn, TXT("FExposureAdaptationPass::Dispatch: missing textures"));
            return;
        }

        // Upload constants
        float ConstantsData[4];
        memset(ConstantsData, 0, sizeof(ConstantsData));
        ConstantsData[0] = Constants.AdaptationSpeed;
        ConstantsData[1] = Constants.KeyValue;

        CmdList->writeBuffer(ConstantBuffer, ConstantsData, sizeof(ConstantsData));

        // Create binding set
        nvrhi::BindingSetDesc BindingSetDesc;
        BindingSetDesc.bindings = {
            nvrhi::BindingSetItem::ConstantBuffer(256, ConstantBuffer),
            nvrhi::BindingSetItem::Texture_SRV(0, Desc.SceneColorTexture),
            nvrhi::BindingSetItem::Sampler(128, LinearSampler),
            nvrhi::BindingSetItem::Texture_UAV(384, Desc.AdaptedLuminanceTexture)
        };
        nvrhi::BindingSetHandle BindingSet = Device->createBindingSet(BindingSetDesc, BindingLayout);

        // Dispatch 1x1x1 for 1x1 output texture
        nvrhi::ComputeState ComputeState;
        ComputeState.setPipeline(Pipeline);
        ComputeState.addBindingSet(BindingSet);
        CmdList->setComputeState(ComputeState);
        CmdList->dispatch(1, 1, 1);
    }

    void FExposureAdaptationPass::Shutdown()
    {
        HLVM_LOG(LogPostProcess, info, TXT("FExposureAdaptationPass::Shutdown"));

        Pipeline = nullptr;
        BindingLayout = nullptr;
        ComputeShader = nullptr;
        ConstantBuffer = nullptr;
        LinearSampler = nullptr;
        Device = nullptr;
        bIsInitialized = false;
    }
}
