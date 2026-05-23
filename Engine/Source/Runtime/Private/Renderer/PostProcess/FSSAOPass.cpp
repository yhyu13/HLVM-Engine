// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/PostProcess/FSSAOPass.h"
#include "Renderer/Shader/ShaderLibrary.h"
#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogPostProcess)

namespace SSao
{
    FSSAOPass::FSSAOPass() = default;
    FSSAOPass::~FSSAOPass() { Shutdown(); }

    bool FSSAOPass::Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir)
    {
        HLVM_LOG(LogPostProcess, info, TXT("FSSAOPass::Initialize"));

        Device = InDevice;

        ComputeShader = FShaderLibrary::Get().LoadShader(
            Device, InShaderDataDir, TXT("HBAO_cs.sblob"), nvrhi::ShaderType::Compute);
        if (!ComputeShader)
        {
            HLVM_LOG(LogPostProcess, err, TXT("Failed to load HBAO_cs shader"));
            return false;
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

            // b0 -> 256 (HBAOConstants)
            // t0 -> 0   (Depth texture)
            // t1 -> 1   (Normal texture)
            // u0 -> 384 (HBAO output)
            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::ConstantBuffer(256),
                nvrhi::BindingLayoutItem::Texture_SRV(0),
                nvrhi::BindingLayoutItem::Texture_SRV(1),
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
                HLVM_LOG(LogPostProcess, err, TXT("Failed to create HBAO compute pipeline"));
                return false;
            }
        }

        // Create constant buffer (256 bytes)
        {
            nvrhi::BufferDesc BufferDesc;
            BufferDesc.byteSize = sizeof(FHBAOConstants);
            BufferDesc.isConstantBuffer = true;
            BufferDesc.isVolatile = false;
            BufferDesc.keepInitialState = true;
            BufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
            BufferDesc.debugName = "HBAOConstants";
            ConstantBuffer = Device->createBuffer(BufferDesc);
        }

        HLVM_LOG(LogPostProcess, info, TXT("FSSAOPass initialized successfully"));
        return true;
    }

    void FSSAOPass::Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FHBAOConstants& Constants)
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
            HLVM_LOG(LogPostProcess, warn, TXT("FSSAOPass::Dispatch: invalid output dimensions"));
            return;
        }

        // Upload constants
        float ConstantsData[64]; // 256 bytes
        memset(ConstantsData, 0, sizeof(ConstantsData));

        // Copy matrices (column-major, 16 floats each)
        memcpy(&ConstantsData[0],  Constants.ProjMatrix,     64);
        memcpy(&ConstantsData[16], Constants.InvProjMatrix,  64);
        memcpy(&ConstantsData[32], Constants.ViewMatrix,     64);

        // ScreenSize, InvScreenSize
        ConstantsData[48] = Constants.ScreenSize[0];
        ConstantsData[49] = Constants.ScreenSize[1];
        ConstantsData[50] = Constants.InvScreenSize[0];
        ConstantsData[51] = Constants.InvScreenSize[1];

        // Scalar parameters
        ConstantsData[52] = Constants.SampleRadius;
        ConstantsData[53] = Constants.AngleBias;
        ConstantsData[54] = Constants.MaxRadiusPixels;
        ConstantsData[55] = Constants.AttenuationScale;
        ConstantsData[56] = Constants.MinAO;
        // DirectionCount and StepCount are now compile-time #defines in shader

        CmdList->writeBuffer(ConstantBuffer, ConstantsData, sizeof(ConstantsData));

        // Create binding set
        nvrhi::BindingSetDesc BindingSetDesc;
        BindingSetDesc.bindings = {
            nvrhi::BindingSetItem::ConstantBuffer(256, ConstantBuffer),
            nvrhi::BindingSetItem::Texture_SRV(0, Desc.DepthTexture),
            nvrhi::BindingSetItem::Texture_SRV(1, Desc.NormalTexture),
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

    void FSSAOPass::Shutdown()
    {
        HLVM_LOG(LogPostProcess, info, TXT("FSSAOPass::Shutdown"));

        Pipeline = nullptr;
        BindingLayout = nullptr;
        ComputeShader = nullptr;
        ConstantBuffer = nullptr;
        Device = nullptr;
    }
}
