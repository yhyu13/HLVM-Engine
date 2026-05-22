// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/PostProcess/FLensEffectsPass.h"
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
// FLensEffectsPass
// ---------------------------------------------------------------------------

namespace LensEffects
{
    bool FLensEffectsPass::Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir)
    {
        if (bIsInitialized)
        {
            Shutdown();
        }

        HLVM_LOG(LogPostProcess, info, TXT("FLensEffectsPass::Initialize"));

        Device = InDevice;
        ShaderDataDir = InShaderDataDir;

        // Load compute shader from .sblob
        auto ShaderBlob = ReadBinaryFile(
            FPath::Combine(InShaderDataDir, TXT("LensEffects_cs.sblob")).string());
        const void* ShaderBinary = nullptr;
        size_t ShaderBinarySize = 0;
        if (!ShaderMake::FindPermutationInBlob(ShaderBlob.data(), ShaderBlob.size(), nullptr, 0, &ShaderBinary, &ShaderBinarySize))
        {
            HLVM_LOG(LogPostProcess, err, TXT("Failed to extract LensEffects_cs from blob"));
            return false;
        }

        nvrhi::ShaderDesc CSDesc;
        CSDesc.setShaderType(nvrhi::ShaderType::Compute);
        ComputeShader = Device->createShader(CSDesc, ShaderBinary, ShaderBinarySize);
        if (!ComputeShader)
        {
            HLVM_LOG(LogPostProcess, err, TXT("Failed to create LensEffects_cs shader"));
            return false;
        }

        // Create samplers
        {
            nvrhi::SamplerDesc PointDesc;
            PointDesc.setAddressU(nvrhi::SamplerAddressMode::ClampToEdge)
                     .setAddressV(nvrhi::SamplerAddressMode::ClampToEdge)
                     .setAddressW(nvrhi::SamplerAddressMode::ClampToEdge)
                     .setMinFilter(false)
                     .setMagFilter(false)
                     .setMipFilter(false);
            PointSampler = Device->createSampler(PointDesc);

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

            // b0 -> 256 (LensEffectsConstants)
            // t0 -> 0   (SDR texture)
            // s0 -> 128 (PointClamp)
            // s1 -> 129 (LinearClamp)
            // u0 -> 384 (Output)
            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::ConstantBuffer(256),
                nvrhi::BindingLayoutItem::Texture_SRV(0),
                nvrhi::BindingLayoutItem::Sampler(128),
                nvrhi::BindingLayoutItem::Sampler(129),
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
                HLVM_LOG(LogPostProcess, err, TXT("Failed to create lens effects compute pipeline"));
                return false;
            }
        }

        // Create constant buffer
        {
            nvrhi::BufferDesc BufferDesc;
            BufferDesc.byteSize = sizeof(FLensEffectsConstants);
            BufferDesc.isConstantBuffer = true;
            BufferDesc.isVolatile = false;
            BufferDesc.keepInitialState = true;
            BufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
            BufferDesc.debugName = "LensEffectsConstants";
            ConstantBuffer = Device->createBuffer(BufferDesc);
        }

        HLVM_LOG(LogPostProcess, info, TXT("FLensEffectsPass initialized successfully"));
        bIsInitialized = true;
        return true;
    }

    void FLensEffectsPass::Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FLensEffectsConstants& Constants)
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
            HLVM_LOG(LogPostProcess, warn, TXT("FLensEffectsPass::Dispatch: invalid output dimensions"));
            return;
        }

        // Upload constants
        float ConstantsData[12]; // 48 bytes
        memset(ConstantsData, 0, sizeof(ConstantsData));

        ConstantsData[0] = Constants.ChromaticAmount;
        ConstantsData[1] = Constants.VignetteIntensity;
        ConstantsData[2] = Constants.GrainIntensity;
        ConstantsData[3] = static_cast<float>(Constants.FrameIndex);
        ConstantsData[4] = Constants.OutputSize[0];
        ConstantsData[5] = Constants.OutputSize[1];
        ConstantsData[6] = Constants.RcpOutputSize[0];
        ConstantsData[7] = Constants.RcpOutputSize[1];

        CmdList->writeBuffer(ConstantBuffer, ConstantsData, sizeof(ConstantsData));

        // Create binding set
        nvrhi::BindingSetDesc BindingSetDesc;
        BindingSetDesc.bindings = {
            nvrhi::BindingSetItem::ConstantBuffer(256, ConstantBuffer),
            nvrhi::BindingSetItem::Texture_SRV(0, Desc.SDRTexture),
            nvrhi::BindingSetItem::Sampler(128, PointSampler),
            nvrhi::BindingSetItem::Sampler(129, LinearSampler),
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

    void FLensEffectsPass::Shutdown()
    {
        HLVM_LOG(LogPostProcess, info, TXT("FLensEffectsPass::Shutdown"));

        Pipeline = nullptr;
        BindingLayout = nullptr;
        ComputeShader = nullptr;
        ConstantBuffer = nullptr;
        PointSampler = nullptr;
        LinearSampler = nullptr;
        Device = nullptr;
        bIsInitialized = false;
    }
}
