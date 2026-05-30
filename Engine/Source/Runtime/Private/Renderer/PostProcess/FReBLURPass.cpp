// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/PostProcess/FReBLURPass.h"
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
// FReBLURPass
// ---------------------------------------------------------------------------

namespace ReBLUR
{
    bool FReBLURPass::Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir)
    {
        if (bIsInitialized)
        {
            Shutdown();
        }

        HLVM_LOG(LogPostProcess, info, TXT("FReBLURPass::Initialize"));

        Device = InDevice;
        ShaderDataDir = InShaderDataDir;

        // Load compute shader from .sblob
        auto ShaderBlob = ReadBinaryFile(
            FPath::Combine(ShaderDataDir, TXT("ReBLUR_cs.sblob")).string());
        const void* ShaderBinary = nullptr;
        size_t ShaderBinarySize = 0;
        if (!ShaderMake::FindPermutationInBlob(ShaderBlob.data(), ShaderBlob.size(), nullptr, 0, &ShaderBinary, &ShaderBinarySize))
        {
            HLVM_LOG(LogPostProcess, err, TXT("Failed to extract ReBLUR_cs from blob"));
            return false;
        }

        nvrhi::ShaderDesc CSDesc;
        CSDesc.setShaderType(nvrhi::ShaderType::Compute);
        ComputeShader = Device->createShader(CSDesc, ShaderBinary, ShaderBinarySize);
        if (!ComputeShader)
        {
            HLVM_LOG(LogPostProcess, err, TXT("Failed to create ReBLUR_cs shader"));
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

        // Create linear sampler (for history texture sampling)
        {
            nvrhi::SamplerDesc SamplerDesc;
            SamplerDesc.setAddressU(nvrhi::SamplerAddressMode::ClampToEdge)
                .setAddressV(nvrhi::SamplerAddressMode::ClampToEdge)
                .setAddressW(nvrhi::SamplerAddressMode::ClampToEdge)
                .setMinFilter(true)
                .setMagFilter(true)
                .setMipFilter(false);
            LinearSampler = Device->createSampler(SamplerDesc);
        }

        // Create binding layout
        {
            nvrhi::BindingLayoutDesc LayoutDesc;
            LayoutDesc.visibility = nvrhi::ShaderType::Compute;

            nvrhi::VulkanBindingOffsets offsets;
            offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
            LayoutDesc.setBindingOffsets(offsets);

            // b0 -> 256 (Constants)
            // t0 -> 0 (Current radiance + hit distance)
            // t1 -> 1 (History texture - SH encoded)
            // t2 -> 2 (Depth texture)
            // t3 -> 3 (Normal + roughness texture)
            // s0 -> 128 (Point sampler for normals/depth)
            // s1 -> 144 (Linear sampler for history)
            // u0 -> 384 (Output texture)
            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::ConstantBuffer(256),
                nvrhi::BindingLayoutItem::Texture_SRV(0),   // Current radiance
                nvrhi::BindingLayoutItem::Texture_SRV(1),   // History
                nvrhi::BindingLayoutItem::Texture_SRV(2),   // Depth
                nvrhi::BindingLayoutItem::Texture_SRV(3),   // Normal+roughness
                nvrhi::BindingLayoutItem::Sampler(128),    // Point sampler
                nvrhi::BindingLayoutItem::Sampler(129),    // Linear sampler
                nvrhi::BindingLayoutItem::Texture_UAV(384) // Output
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
                HLVM_LOG(LogPostProcess, err, TXT("Failed to create ReBLUR pipeline"));
                return false;
            }
        }

        // Create constant buffer (352 bytes for FReBLURConstants)
        {
            nvrhi::BufferDesc BufferDesc;
            BufferDesc.byteSize = 512; // Pad to 512 for alignment
            BufferDesc.isConstantBuffer = true;
            BufferDesc.isVolatile = false;
            BufferDesc.keepInitialState = true;
            BufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
            BufferDesc.debugName = "ReBLURConstants";
            ConstantBuffer = Device->createBuffer(BufferDesc);
        }

        bIsInitialized = true;
        HLVM_LOG(LogPostProcess, info, TXT("FReBLURPass initialized successfully"));
        return true;
    }

    void FReBLURPass::Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FReBLURConstants& Constants, const FPooledBlurParams& BlurParams)
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
            HLVM_LOG(LogPostProcess, warn, TXT("FReBLURPass::Dispatch: invalid output dimensions"));
            return;
        }

        // Build constants data
        float ConstantsData[128]; // 512 bytes (padded)
        memset(ConstantsData, 0, sizeof(ConstantsData));

        size_t offset = 0;

        // InverseCurrViewProj (16 floats)
        memcpy(&ConstantsData[offset], Constants.InverseCurrViewProj, 64);
        offset += 16;

        // PrevViewProj (16 floats)
        memcpy(&ConstantsData[offset], Constants.PrevViewProj, 64);
        offset += 16;

        // ViewMatrix (16 floats)
        memcpy(&ConstantsData[offset], Constants.ViewMatrix, 64);
        offset += 16;

        // ProjMatrix (16 floats)
        memcpy(&ConstantsData[offset], Constants.ProjMatrix, 64);
        offset += 16;

        // OutputSize (2 floats)
        ConstantsData[offset++] = Constants.OutputSize[0];
        ConstantsData[offset++] = Constants.OutputSize[1];

        // RcpOutputSize (2 floats)
        ConstantsData[offset++] = Constants.RcpOutputSize[0];
        ConstantsData[offset++] = Constants.RcpOutputSize[1];

        // HitDistParams (4 floats: A, B, C, D)
        ConstantsData[offset++] = Constants.HitDistParams[0];
        ConstantsData[offset++] = Constants.HitDistParams[1];
        ConstantsData[offset++] = Constants.HitDistParams[2];
        ConstantsData[offset++] = Constants.HitDistParams[3];

        // BlurRadius, NormalWeight, PlaneWeight, RoughnessWeight
        ConstantsData[offset++] = BlurParams.BlurRadius;
        ConstantsData[offset++] = BlurParams.NormalWeight;
        ConstantsData[offset++] = BlurParams.PlaneWeight;
        ConstantsData[offset++] = BlurParams.RoughnessWeight;

        // AntiLagIntensity, DarknessSensitivity, FrameIndex, HistoryFadeIn
        ConstantsData[offset++] = BlurParams.AntiLagIntensity;
        ConstantsData[offset++] = BlurParams.DarknessSensitivity;
        ConstantsData[offset++] = Constants.FrameIndex;
        ConstantsData[offset++] = Constants.HistoryFadeIn;

        // ConfidenceScale, PassIndex, then 2 padding to 16-byte boundary
        ConstantsData[offset++] = Constants.ConfidenceScale;
        ConstantsData[offset++] = Constants.PassIndex;
        offset += 2;

        CmdList->writeBuffer(ConstantBuffer, ConstantsData, sizeof(ConstantsData));

        // Create binding set
        nvrhi::BindingSetDesc BindingSetDesc;
        BindingSetDesc.bindings = {
            nvrhi::BindingSetItem::ConstantBuffer(256, ConstantBuffer),
            nvrhi::BindingSetItem::Texture_SRV(0, Desc.CurrentRadianceTexture),
            nvrhi::BindingSetItem::Texture_SRV(1, Desc.HistoryTexture),
            nvrhi::BindingSetItem::Texture_SRV(2, Desc.DepthTexture),
            nvrhi::BindingSetItem::Texture_SRV(3, Desc.NormalRoughnessTexture),
            nvrhi::BindingSetItem::Sampler(128, PointSampler),
            nvrhi::BindingSetItem::Sampler(129, LinearSampler),
            nvrhi::BindingSetItem::Texture_UAV(384, Desc.OutputTexture)
        };
        nvrhi::BindingSetHandle BindingSet = Device->createBindingSet(BindingSetDesc, BindingLayout);

        // Dispatch (8x8 thread groups)
        uint32_t dispatchX = (outputW + 7) / 8;
        uint32_t dispatchY = (outputH + 7) / 8;

        nvrhi::ComputeState ComputeState;
        ComputeState.setPipeline(Pipeline);
        ComputeState.addBindingSet(BindingSet);
        CmdList->setComputeState(ComputeState);
        CmdList->dispatch(dispatchX, dispatchY, 1);
    }

    void FReBLURPass::Shutdown()
    {
        HLVM_LOG(LogPostProcess, info, TXT("FReBLURPass::Shutdown"));

        if (Pipeline)
        {
            Pipeline = nullptr;
        }
        if (BindingLayout)
        {
            BindingLayout = nullptr;
        }
        if (ComputeShader)
        {
            ComputeShader = nullptr;
        }
        if (ConstantBuffer)
        {
            ConstantBuffer = nullptr;
        }
        if (PointSampler)
        {
            PointSampler = nullptr;
        }
        if (LinearSampler)
        {
            LinearSampler = nullptr;
        }
        Device = nullptr;
        bIsInitialized = false;
    }
} // namespace ReBLUR
