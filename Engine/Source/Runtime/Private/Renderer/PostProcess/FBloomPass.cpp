// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/PostProcess/FBloomPass.h"
#include "Core/Log.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
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
// FBloomPass
// ---------------------------------------------------------------------------

namespace
{
    struct FBloomConstants
    {
        TFP32 FullResSize[2];
        TFP32 HalfResSize[2];
        TFP32 Threshold;
        TFP32 Intensity;
        TFP32 Sigma;
        TFP32 Direction;
        TINT32 BlurIterations;
        TINT32 Pad0;
        TFP32 Pad1[54];
    };
    static_assert(sizeof(FBloomConstants) == 256, "FBloomConstants must be 256 bytes");
}

bool FBloomPass::Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir)
{
    HLVM_LOG(LogPostProcess, info, TXT("FBloomPass::Initialize"));

    Device = InDevice;
    ShaderDataDir = InShaderDataDir;

    // Load shaders from .sblob
    auto LoadShader = [&](const FString& Name, nvrhi::ShaderHandle& OutShader) -> bool
    {
        auto Blob = ReadBinaryFile(FPath::Combine(ShaderDataDir, Name).string());
        const void* Binary = nullptr;
        size_t BinarySize = 0;
        if (!ShaderMake::FindPermutationInBlob(Blob.data(), Blob.size(), nullptr, 0, &Binary, &BinarySize))
        {
            HLVM_LOG(LogPostProcess, err, TXT("Failed to extract {} from blob"), *Name);
            return false;
        }

        nvrhi::ShaderDesc Desc;
        Desc.setShaderType(nvrhi::ShaderType::Compute);
        OutShader = Device->createShader(Desc, Binary, BinarySize);
        if (!OutShader)
        {
            HLVM_LOG(LogPostProcess, err, TXT("Failed to create {} shader"), *Name);
            return false;
        }
        return true;
    };

    if (!LoadShader(TXT("BloomThresholdDownsample_cs.sblob"), ThresholdShader))
        return false;
    if (!LoadShader(TXT("BloomGaussianBlur_cs.sblob"), BlurShader))
        return false;
    if (!LoadShader(TXT("BloomUpsample_cs.sblob"), UpsampleShader))
        return false;

    // Create shared binding layout
    {
        nvrhi::BindingLayoutDesc LayoutDesc;
        LayoutDesc.visibility = nvrhi::ShaderType::Compute;

        nvrhi::VulkanBindingOffsets offsets;
        offsets.setConstantBufferOffset(0)
               .setShaderResourceOffset(0)
               .setSamplerOffset(0)
               .setUnorderedAccessViewOffset(0);
        LayoutDesc.setBindingOffsets(offsets);

        // b0 -> 256 (Constants)
        // t0 -> 0   (Input texture)
        // s0 -> 128 (Linear sampler)
        // u0 -> 384 (Output texture)
        LayoutDesc.bindings = {
            nvrhi::BindingLayoutItem::ConstantBuffer(256),
            nvrhi::BindingLayoutItem::Texture_SRV(0),
            nvrhi::BindingLayoutItem::Sampler(128),
            nvrhi::BindingLayoutItem::Texture_UAV(384)
        };

        BindingLayout = Device->createBindingLayout(LayoutDesc);
    }

    // Create compute pipelines
    {
        nvrhi::ComputePipelineDesc PipelineDesc;
        PipelineDesc.setComputeShader(ThresholdShader);
        PipelineDesc.addBindingLayout(BindingLayout);
        ThresholdPipeline = Device->createComputePipeline(PipelineDesc);
        if (!ThresholdPipeline)
        {
            HLVM_LOG(LogPostProcess, err, TXT("Failed to create bloom threshold pipeline"));
            return false;
        }

        PipelineDesc.setComputeShader(BlurShader);
        BlurPipeline = Device->createComputePipeline(PipelineDesc);
        if (!BlurPipeline)
        {
            HLVM_LOG(LogPostProcess, err, TXT("Failed to create bloom blur pipeline"));
            return false;
        }

        PipelineDesc.setComputeShader(UpsampleShader);
        UpsamplePipeline = Device->createComputePipeline(PipelineDesc);
        if (!UpsamplePipeline)
        {
            HLVM_LOG(LogPostProcess, err, TXT("Failed to create bloom upsample pipeline"));
            return false;
        }
    }

    // Create constant buffer
    {
        nvrhi::BufferDesc BufferDesc;
        BufferDesc.byteSize = sizeof(FBloomConstants);
        BufferDesc.isConstantBuffer = true;
        BufferDesc.isVolatile = false;
        BufferDesc.keepInitialState = true;
        BufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
        BufferDesc.debugName = "BloomConstants";
        ConstantBuffer = Device->createBuffer(BufferDesc);
    }

    HLVM_LOG(LogPostProcess, info, TXT("FBloomPass initialized successfully"));
    return true;
}

void FBloomPass::Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc)
{
    if (!CmdList || !ThresholdPipeline || !BlurPipeline || !UpsamplePipeline || !ConstantBuffer)
        return;

    uint32_t fullW = Desc.FullResWidth;
    uint32_t fullH = Desc.FullResHeight;
    if (!fullW || !fullH)
    {
        HLVM_LOG(LogPostProcess, warn, TXT("FBloomPass::Dispatch: invalid full-res dimensions"));
        return;
    }

    uint32_t halfW = std::max(1u, fullW / 2);
    uint32_t halfH = std::max(1u, fullH / 2);

    // Helper to upload constants
    auto UploadConstants = [&](float Direction)
    {
        FBloomConstants Constants;
        memset(&Constants, 0, sizeof(Constants));
        Constants.FullResSize[0] = static_cast<TFP32>(fullW);
        Constants.FullResSize[1] = static_cast<TFP32>(fullH);
        Constants.HalfResSize[0] = static_cast<TFP32>(halfW);
        Constants.HalfResSize[1] = static_cast<TFP32>(halfH);
        Constants.Threshold = Desc.Threshold;
        Constants.Intensity = Desc.Intensity;
        Constants.Sigma = Desc.Sigma;
        Constants.Direction = Direction;
        Constants.BlurIterations = Desc.BlurIterations;
        CmdList->writeBuffer(ConstantBuffer, &Constants, sizeof(Constants));
    };

    // Helper to create binding set and dispatch
    auto DispatchPass = [&](nvrhi::ComputePipelineHandle Pipeline,
                            nvrhi::TextureHandle InputTexture,
                            nvrhi::TextureHandle OutputTexture,
                            uint32_t DispatchW, uint32_t DispatchH)
    {
        nvrhi::BindingSetDesc BindingSetDesc;
        BindingSetDesc.bindings = {
            nvrhi::BindingSetItem::ConstantBuffer(256, ConstantBuffer),
            nvrhi::BindingSetItem::Texture_SRV(0, InputTexture),
            nvrhi::BindingSetItem::Sampler(128, Desc.LinearSampler),
            nvrhi::BindingSetItem::Texture_UAV(384, OutputTexture)
        };
        nvrhi::BindingSetHandle BindingSet = Device->createBindingSet(BindingSetDesc, BindingLayout);

        uint32_t dispatchX = (DispatchW + 7) / 8;
        uint32_t dispatchY = (DispatchH + 7) / 8;

        nvrhi::ComputeState ComputeState;
        ComputeState.setPipeline(Pipeline);
        ComputeState.addBindingSet(BindingSet);
        CmdList->setComputeState(ComputeState);
        CmdList->dispatch(dispatchX, dispatchY, 1);
    };

    // =====================================================================
    // Step 1: Threshold + Downsample
    // =====================================================================
    UploadConstants(0.0f);
    DispatchPass(ThresholdPipeline, Desc.HDRTexture, Desc.HalfResTexture, halfW, halfH);

    // Transition HalfResTexture (UAV written) → SRV for blur read
    CmdList->setTextureState(Desc.HalfResTexture, nvrhi::AllSubresources,
                             nvrhi::ResourceStates::ShaderResource);

    // =====================================================================
    // Step 2: Gaussian Blur (ping-pong)
    // =====================================================================
    nvrhi::TextureHandle src = Desc.HalfResTexture;
    nvrhi::TextureHandle dst = Desc.BlurTempTexture;

    for (int32_t i = 0; i < Desc.BlurIterations; i++)
    {
        // Horizontal pass
        UploadConstants(1.0f);
        DispatchPass(BlurPipeline, src, dst, halfW, halfH);

        // Transition: dst (UAV written) → SRV, src (was SRV) → UAV
        CmdList->setTextureState(dst, nvrhi::AllSubresources,
                                 nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(src, nvrhi::AllSubresources,
                                 nvrhi::ResourceStates::UnorderedAccess);
        std::swap(src, dst);

        // Vertical pass
        UploadConstants(0.0f);
        DispatchPass(BlurPipeline, src, dst, halfW, halfH);

        // Transition for next iteration (if not last)
        if (i < Desc.BlurIterations - 1)
        {
            CmdList->setTextureState(dst, nvrhi::AllSubresources,
                                     nvrhi::ResourceStates::ShaderResource);
            CmdList->setTextureState(src, nvrhi::AllSubresources,
                                     nvrhi::ResourceStates::UnorderedAccess);
            std::swap(src, dst);
        }
    }

    // After loop: 'dst' contains final blurred result
    // =====================================================================
    // Step 3: Upsample to full res
    // =====================================================================
    // dst must be SRV, OutputTexture must be UAV
    CmdList->setTextureState(dst, nvrhi::AllSubresources,
                             nvrhi::ResourceStates::ShaderResource);
    CmdList->setTextureState(Desc.OutputTexture, nvrhi::AllSubresources,
                             nvrhi::ResourceStates::UnorderedAccess);

    UploadConstants(0.0f);
    DispatchPass(UpsamplePipeline, dst, Desc.OutputTexture, fullW, fullH);
}

void FBloomPass::Shutdown()
{
    HLVM_LOG(LogPostProcess, info, TXT("FBloomPass::Shutdown"));

    UpsamplePipeline = nullptr;
    BlurPipeline = nullptr;
    ThresholdPipeline = nullptr;
    BindingLayout = nullptr;
    UpsampleShader = nullptr;
    BlurShader = nullptr;
    ThresholdShader = nullptr;
    ConstantBuffer = nullptr;
    Device = nullptr;
}
