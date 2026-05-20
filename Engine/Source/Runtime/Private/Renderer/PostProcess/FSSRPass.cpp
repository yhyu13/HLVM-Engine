// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/PostProcess/FSSRPass.h"
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
// FSSRPass
// ---------------------------------------------------------------------------

namespace SSr
{
    bool FSSRPass::Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir)
    {
        if (bIsInitialized)
        {
            Shutdown();
        }

        HLVM_LOG(LogPostProcess, info, TXT("FSSRPass::Initialize"));

        Device = InDevice;
        ShaderDataDir = InShaderDataDir;

        // Load compute shader from .sblob
        auto ShaderBlob = ReadBinaryFile(
            FPath::Combine(InShaderDataDir, TXT("SSR_cs.sblob")).string());
        const void* ShaderBinary = nullptr;
        size_t ShaderBinarySize = 0;
        if (!ShaderMake::FindPermutationInBlob(ShaderBlob.data(), ShaderBlob.size(), nullptr, 0, &ShaderBinary, &ShaderBinarySize))
        {
            HLVM_LOG(LogPostProcess, err, TXT("Failed to extract SSR_cs from blob"));
            return false;
        }

        nvrhi::ShaderDesc CSDesc;
        CSDesc.setShaderType(nvrhi::ShaderType::Compute);
        ComputeShader = Device->createShader(CSDesc, ShaderBinary, ShaderBinarySize);
        if (!ComputeShader)
        {
            HLVM_LOG(LogPostProcess, err, TXT("Failed to create SSR_cs shader"));
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

            // b0 -> 256 (SSRConstants)
            // t0 -> 0   (Depth texture)
            // t1 -> 1   (Normal texture)
            // t2 -> 2   (Material texture)
            // t3 -> 3   (HDR texture)
            // s0 -> 128 (Point sampler)
            // s1 -> 129 (Linear sampler)
            // u0 -> 384 (SSR output)
            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::ConstantBuffer(256),
                nvrhi::BindingLayoutItem::Texture_SRV(0),
                nvrhi::BindingLayoutItem::Texture_SRV(1),
                nvrhi::BindingLayoutItem::Texture_SRV(2),
                nvrhi::BindingLayoutItem::Texture_SRV(3),
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
                HLVM_LOG(LogPostProcess, err, TXT("Failed to create SSR compute pipeline"));
                return false;
            }
        }

        // Create constant buffer (256 bytes)
        {
            nvrhi::BufferDesc BufferDesc;
            BufferDesc.byteSize = sizeof(FSSRConstants);
            BufferDesc.isConstantBuffer = true;
            BufferDesc.isVolatile = false;
            BufferDesc.keepInitialState = true;
            BufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
            BufferDesc.debugName = "SSRConstants";
            ConstantBuffer = Device->createBuffer(BufferDesc);
        }

        HLVM_LOG(LogPostProcess, info, TXT("FSSRPass initialized successfully"));
        bIsInitialized = true;
        return true;
    }

    void FSSRPass::Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FSSRConstants& Constants)
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
            HLVM_LOG(LogPostProcess, warn, TXT("FSSRPass::Dispatch: invalid output dimensions"));
            return;
        }

        // Upload constants
        float ConstantsData[64]; // 256 bytes
        memset(ConstantsData, 0, sizeof(ConstantsData));

        // Copy matrices (column-major, 16 floats each)
        memcpy(&ConstantsData[0],  Constants.ProjMatrix,     64);
        memcpy(&ConstantsData[16], Constants.InvProjMatrix,  64);
        memcpy(&ConstantsData[32], Constants.ViewMatrix,     64);

        // Screen sizes
        ConstantsData[48] = Constants.FullScreenSize[0];
        ConstantsData[49] = Constants.FullScreenSize[1];
        ConstantsData[50] = Constants.InvFullScreenSize[0];
        ConstantsData[51] = Constants.InvFullScreenSize[1];
        ConstantsData[52] = Constants.HalfScreenSize[0];
        ConstantsData[53] = Constants.HalfScreenSize[1];
        ConstantsData[54] = Constants.InvHalfScreenSize[0];
        ConstantsData[55] = Constants.InvHalfScreenSize[1];

        // Scalar parameters
        ConstantsData[56] = static_cast<float>(Constants.MaxSteps);
        ConstantsData[57] = Constants.StepSize;
        ConstantsData[58] = Constants.MaxDistance;
        ConstantsData[59] = Constants.Thickness;

        CmdList->writeBuffer(ConstantBuffer, ConstantsData, sizeof(ConstantsData));

        // Create binding set
        nvrhi::BindingSetDesc BindingSetDesc;
        BindingSetDesc.bindings = {
            nvrhi::BindingSetItem::ConstantBuffer(256, ConstantBuffer),
            nvrhi::BindingSetItem::Texture_SRV(0, Desc.DepthTexture),
            nvrhi::BindingSetItem::Texture_SRV(1, Desc.NormalTexture),
            nvrhi::BindingSetItem::Texture_SRV(2, Desc.MaterialTexture),
            nvrhi::BindingSetItem::Texture_SRV(3, Desc.HDRTexture),
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

    void FSSRPass::Shutdown()
    {
        HLVM_LOG(LogPostProcess, info, TXT("FSSRPass::Shutdown"));

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
