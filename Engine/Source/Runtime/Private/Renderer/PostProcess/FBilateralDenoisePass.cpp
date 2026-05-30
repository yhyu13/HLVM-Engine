// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/PostProcess/FBilateralDenoisePass.h"
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
// FBilateralDenoisePass
// ---------------------------------------------------------------------------

bool FBilateralDenoisePass::Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir)
{
    HLVM_LOG(LogPostProcess, info, TXT("FBilateralDenoisePass::Initialize"));

    Device = InDevice;
    ShaderDataDir = InShaderDataDir;

    // Load compute shader from .sblob
    auto ShaderBlob = ReadBinaryFile(
        FPath::Combine(ShaderDataDir, TXT("BilateralDenoise_cs.sblob")).string());
    const void* ShaderBinary = nullptr;
    size_t ShaderBinarySize = 0;
    if (!ShaderMake::FindPermutationInBlob(ShaderBlob.data(), ShaderBlob.size(), nullptr, 0, &ShaderBinary, &ShaderBinarySize))
    {
        HLVM_LOG(LogPostProcess, err, TXT("Failed to extract BilateralDenoise_cs from blob"));
        return false;
    }

    nvrhi::ShaderDesc CSDesc;
    CSDesc.setShaderType(nvrhi::ShaderType::Compute);
    Shader = Device->createShader(CSDesc, ShaderBinary, ShaderBinarySize);
    if (!Shader)
    {
        HLVM_LOG(LogPostProcess, err, TXT("Failed to create BilateralDenoise_cs shader"));
        return false;
    }

    // Create point sampler (nearest-neighbor for exact pixel sampling)
    {
        nvrhi::SamplerDesc SamplerDesc;
        SamplerDesc.setAddressU(nvrhi::SamplerAddressMode::ClampToEdge)
            .setAddressV(nvrhi::SamplerAddressMode::ClampToEdge)
            .setAddressW(nvrhi::SamplerAddressMode::ClampToEdge)
            .setMinFilter(false)
            .setMagFilter(false)
            .setMipFilter(false);
        PointSampler = Device->createSampler(SamplerDesc);
    }

    // Create binding layout
    {
        nvrhi::BindingLayoutDesc LayoutDesc;
        LayoutDesc.visibility = nvrhi::ShaderType::Compute;

        nvrhi::VulkanBindingOffsets offsets;
        offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
        LayoutDesc.setBindingOffsets(offsets);

        // b0 -> 256 (Constants)
        // t0 -> 0 (Input texture - RGB)
        // t1 -> 1 (Depth guide)
        // t2 -> 2 (Normal guide - optional)
        // s0 -> 128 (Point sampler)
        // u0 -> 384 (Output texture - RGB)
        LayoutDesc.bindings = {
            nvrhi::BindingLayoutItem::ConstantBuffer(256),
            nvrhi::BindingLayoutItem::Texture_SRV(0),   // Input RGB
            nvrhi::BindingLayoutItem::Texture_SRV(1),   // Depth
            nvrhi::BindingLayoutItem::Texture_SRV(2),   // Normal
            nvrhi::BindingLayoutItem::Sampler(128),
            nvrhi::BindingLayoutItem::Texture_UAV(384) // Output RGB
        };

        BindingLayout = Device->createBindingLayout(LayoutDesc);
    }

    // Create compute pipeline
    {
        nvrhi::ComputePipelineDesc PipelineDesc;
        PipelineDesc.setComputeShader(Shader);
        PipelineDesc.addBindingLayout(BindingLayout);

        Pipeline = Device->createComputePipeline(PipelineDesc);
        if (!Pipeline)
        {
            HLVM_LOG(LogPostProcess, err, TXT("Failed to create BilateralDenoise pipeline"));
            return false;
        }
    }

    // Create constant buffer (6 float4s = 96 bytes, padded to 256 for alignment)
    {
        nvrhi::BufferDesc BufferDesc;
        BufferDesc.byteSize = 256;
        BufferDesc.isConstantBuffer = true;
        BufferDesc.isVolatile = false;
        BufferDesc.keepInitialState = true;
        BufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
        BufferDesc.debugName = "BilateralDenoiseConstants";
        ConstantBuffer = Device->createBuffer(BufferDesc);
    }

    HLVM_LOG(LogPostProcess, info, TXT("FBilateralDenoisePass initialized successfully"));
    return true;
}

void FBilateralDenoisePass::Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc)
{
    if (!CmdList || !Pipeline || !ConstantBuffer)
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
        HLVM_LOG(LogPostProcess, warn, TXT("FBilateralDenoisePass::Dispatch: invalid output dimensions"));
        return;
    }

    // Upload constants
    float ConstantsData[64]; // 256 bytes
    memset(ConstantsData, 0, sizeof(ConstantsData));
    ConstantsData[0] = 1.0f / static_cast<float>(outputW);  // TexelSizeX
    ConstantsData[1] = 1.0f / static_cast<float>(outputH); // TexelSizeY
    ConstantsData[2] = Desc.DepthSigma;
    ConstantsData[3] = Desc.NormalSigma;
    ConstantsData[4] = Desc.SpatialSigma;
    // Pad[5], Pad[6], Pad[7] remain zero
    CmdList->writeBuffer(ConstantBuffer, ConstantsData, sizeof(ConstantsData));

    // Create binding set
    nvrhi::BindingSetDesc BindingSetDesc;
    BindingSetDesc.bindings = {
        nvrhi::BindingSetItem::ConstantBuffer(256, ConstantBuffer),
        nvrhi::BindingSetItem::Texture_SRV(0, Desc.InputTexture),
        nvrhi::BindingSetItem::Texture_SRV(1, Desc.DepthTexture),
        nvrhi::BindingSetItem::Texture_SRV(2, Desc.NormalTexture),
        nvrhi::BindingSetItem::Sampler(128, PointSampler),
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

void FBilateralDenoisePass::Shutdown()
{
    HLVM_LOG(LogPostProcess, info, TXT("FBilateralDenoisePass::Shutdown"));

    Pipeline = nullptr;
    BindingLayout = nullptr;
    Shader = nullptr;
    ConstantBuffer = nullptr;
    PointSampler = nullptr;
    Device = nullptr;
}
