// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/PostProcess/FReBLURPass.h"
#include "Core/Log.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
#include "Renderer/Common/FBindingLayoutBuilder.h"
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

        // Create binding layout using FBindingLayoutBuilder (eliminates manual shift math)
        {
            FBindingLayoutBuilder Builder;
            Builder.SetVisibility(nvrhi::ShaderType::Compute)
                .AddConstantBuffer(0)       // b0 → binding 256 (Constants)
                .AddTextureSRV(0)           // t0 → binding 0   (Current radiance)
                .AddTextureSRV(1)           // t1 → binding 1   (History)
                .AddTextureSRV(2)           // t2 → binding 2   (Depth)
                .AddTextureSRV(3)           // t3 → binding 3   (Normal+roughness)
                .AddSampler(0)              // s0 → binding 128 (Point sampler)
                .AddSampler(1)              // s1 → binding 129 (Linear sampler)
                .AddTextureUAV(0);          // u0 → binding 384 (Output)

            nvrhi::BindingLayoutDesc LayoutDesc = Builder.Build();
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

        // ConfidenceScale, SpatialAlpha, PassIndex, then 2 padding to 16-byte boundary
        ConstantsData[offset++] = Constants.ConfidenceScale;
        ConstantsData[offset++] = Constants.SpatialAlpha;
        ConstantsData[offset++] = Constants.PassIndex;
        offset += 2;

        CmdList->writeBuffer(ConstantBuffer, ConstantsData, sizeof(ConstantsData));

        // Create binding set using FBindingSetBuilder (mirrors layout builder shift math)
        FBindingSetBuilder SetBuilder;
        SetBuilder.SetConstantBuffer(0, ConstantBuffer)
            .SetTextureSRV(0, Desc.CurrentRadianceTexture)
            .SetTextureSRV(1, Desc.HistoryTexture)
            .SetTextureSRV(2, Desc.DepthTexture)
            .SetTextureSRV(3, Desc.NormalRoughnessTexture)
            .SetSampler(0, PointSampler)
            .SetSampler(1, LinearSampler)
            .SetTextureUAV(0, Desc.OutputTexture);

        nvrhi::BindingSetDesc BindingSetDesc = SetBuilder.Build();

        // Debug validation: assert binding set matches layout (catches b0/b1/b257/b512 class bugs)
        const nvrhi::BindingLayoutDesc* ExpectedLayout = BindingLayout->getDesc();
        HLVM_ENSURE(ExpectedLayout != nullptr);
        HLVM_ENSURE(FBindingSetBuilder::ValidateAgainstLayout(BindingSetDesc, *ExpectedLayout));

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

    static void EnsureDummyTexture(nvrhi::IDevice* Device, nvrhi::TextureHandle& Texture, uint32_t W, uint32_t H,
                                   nvrhi::Format Format, const char* DebugName)
    {
        bool bNeedsRecreate = true;
        if (Texture)
        {
            auto Desc = Texture->getDesc();
            bNeedsRecreate = (Desc.width != W || Desc.height != H || Desc.format != Format);
        }

        if (bNeedsRecreate)
        {
            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = W;
            Desc.height = H;
            Desc.format = Format;
            Desc.isUAV = true;
            Desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
            Desc.keepInitialState = true;
            Desc.debugName = DebugName;
            Texture = Device->createTexture(Desc);
        }
    }

    void FReBLURPass::Dispatch(nvrhi::ICommandList* CmdList, nvrhi::TextureHandle Input,
                               nvrhi::TextureHandle Output, uint32_t W, uint32_t H)
    {
        if (!CmdList || !Device || !Input || !Output || W == 0 || H == 0)
            return;

        // Dummy depth = 1.0 (valid, no early-out), dummy normal = (0,0,1), roughness = 1.0
        EnsureDummyTexture(Device, DummyDepthTexture, W, H, nvrhi::Format::R32_FLOAT, "ReBLUR.DummyDepth");
        EnsureDummyTexture(Device, DummyNormalTexture, W, H, nvrhi::Format::RGBA8_UNORM, "ReBLUR.DummyNormal");

        FDesc Desc;
        Desc.CurrentRadianceTexture = Input;
        Desc.HistoryTexture = Input; // No temporal history; use current as history
        Desc.DepthTexture = DummyDepthTexture;
        Desc.NormalRoughnessTexture = DummyNormalTexture;
        Desc.OutputTexture = Output;
        Desc.OutputWidth = W;
        Desc.OutputHeight = H;

        FReBLURConstants Constants{};
        Constants.OutputSize[0] = static_cast<TFP32>(W);
        Constants.OutputSize[1] = static_cast<TFP32>(H);
        Constants.RcpOutputSize[0] = 1.0f / static_cast<TFP32>(W);
        Constants.RcpOutputSize[1] = 1.0f / static_cast<TFP32>(H);
        Constants.HitDistParams[0] = 3.0f;
        Constants.HitDistParams[1] = 0.1f;
        Constants.HitDistParams[2] = 20.0f;
        Constants.HitDistParams[3] = -25.0f;
        Constants.FrameIndex = 0.0f;
        Constants.HistoryFadeIn = 1.0f;
        Constants.ConfidenceScale = 1.0f;
        Constants.PassIndex = 0.0f;

        FPooledBlurParams BlurParams = GetDefaultBlurParams();
        Dispatch(CmdList, Desc, Constants, BlurParams);
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
        if (DummyDepthTexture)
        {
            DummyDepthTexture = nullptr;
        }
        if (DummyNormalTexture)
        {
            DummyNormalTexture = nullptr;
        }
        Device = nullptr;
        bIsInitialized = false;
    }
} // namespace ReBLUR
