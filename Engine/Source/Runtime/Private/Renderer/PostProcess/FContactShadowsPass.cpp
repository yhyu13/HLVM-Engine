// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/PostProcess/FContactShadowsPass.h"
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
// FContactShadowsPass
// ---------------------------------------------------------------------------

namespace ContactShadows
{
    bool FContactShadowsPass::Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir)
    {
        if (bIsInitialized)
        {
            Shutdown();
        }

        HLVM_LOG(LogPostProcess, info, TXT("FContactShadowsPass::Initialize"));

        Device = InDevice;
        ShaderDataDir = InShaderDataDir;

        // Load compute shader from .sblob
        auto ShaderBlob = ReadBinaryFile(
            FPath::Combine(InShaderDataDir, TXT("ContactShadows_cs.sblob")).string());
        const void* ShaderBinary = nullptr;
        size_t ShaderBinarySize = 0;
        if (!ShaderMake::FindPermutationInBlob(ShaderBlob.data(), ShaderBlob.size(), nullptr, 0, &ShaderBinary, &ShaderBinarySize))
        {
            HLVM_LOG(LogPostProcess, err, TXT("Failed to extract ContactShadows_cs from blob"));
            return false;
        }

        nvrhi::ShaderDesc CSDesc;
        CSDesc.setShaderType(nvrhi::ShaderType::Compute);
        ComputeShader = Device->createShader(CSDesc, ShaderBinary, ShaderBinarySize);
        if (!ComputeShader)
        {
            HLVM_LOG(LogPostProcess, err, TXT("Failed to create ContactShadows_cs shader"));
            return false;
        }

        // Create point sampler
        {
            nvrhi::SamplerDesc PointDesc;
            PointDesc.setAddressU(nvrhi::SamplerAddressMode::ClampToEdge)
                     .setAddressV(nvrhi::SamplerAddressMode::ClampToEdge)
                     .setAddressW(nvrhi::SamplerAddressMode::ClampToEdge)
                     .setMinFilter(false)
                     .setMagFilter(false)
                     .setMipFilter(false);
            PointSampler = Device->createSampler(PointDesc);
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
                HLVM_LOG(LogPostProcess, err, TXT("Failed to create contact shadows compute pipeline"));
                return false;
            }
        }

        // Create constant buffer
        {
            nvrhi::BufferDesc BufferDesc;
            BufferDesc.byteSize = sizeof(FContactShadowConstants);
            BufferDesc.isConstantBuffer = true;
            BufferDesc.isVolatile = false;
            BufferDesc.keepInitialState = true;
            BufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
            BufferDesc.debugName = "ContactShadowConstants";
            ConstantBuffer = Device->createBuffer(BufferDesc);
        }

        HLVM_LOG(LogPostProcess, info, TXT("FContactShadowsPass initialized successfully"));
        bIsInitialized = true;
        return true;
    }

    void FContactShadowsPass::Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FContactShadowConstants& Constants)
    {
        if (!bIsInitialized || !CmdList || !Pipeline || !ConstantBuffer)
            return;

        if (!Desc.DepthTexture || !Desc.OutputTexture)
        {
            HLVM_LOG(LogPostProcess, warn, TXT("FContactShadowsPass::Dispatch: missing textures"));
            return;
        }

        // Upload constants
        float ConstantsData[64];
        memset(ConstantsData, 0, sizeof(ConstantsData));
        memcpy(ConstantsData, &Constants, sizeof(FContactShadowConstants));

        CmdList->writeBuffer(ConstantBuffer, ConstantsData, sizeof(ConstantsData));

        // Create binding set
        nvrhi::BindingSetDesc BindingSetDesc;
        BindingSetDesc.bindings = {
            nvrhi::BindingSetItem::ConstantBuffer(256, ConstantBuffer),
            nvrhi::BindingSetItem::Texture_SRV(0, Desc.DepthTexture),
            nvrhi::BindingSetItem::Sampler(128, PointSampler),
            nvrhi::BindingSetItem::Texture_UAV(384, Desc.OutputTexture)
        };
        nvrhi::BindingSetHandle BindingSet = Device->createBindingSet(BindingSetDesc, BindingLayout);

        // Dispatch
        uint32_t dispatchX = (Desc.Width + 7) / 8;
        uint32_t dispatchY = (Desc.Height + 7) / 8;

        nvrhi::ComputeState ComputeState;
        ComputeState.setPipeline(Pipeline);
        ComputeState.addBindingSet(BindingSet);
        CmdList->setComputeState(ComputeState);
        CmdList->dispatch(dispatchX, dispatchY, 1);
    }

    void FContactShadowsPass::Shutdown()
    {
        HLVM_LOG(LogPostProcess, info, TXT("FContactShadowsPass::Shutdown"));

        Pipeline = nullptr;
        BindingLayout = nullptr;
        ComputeShader = nullptr;
        ConstantBuffer = nullptr;
        PointSampler = nullptr;
        Device = nullptr;
        bIsInitialized = false;
    }
}
