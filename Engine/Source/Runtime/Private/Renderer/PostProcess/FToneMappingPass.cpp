// Copyright 2026 HLVM Engine
//
// MIT License
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "Renderer/PostProcess/FToneMappingPass.h"
#include "Renderer/Shader/ShaderLibrary.h"
#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogRenderer)

bool FToneMappingPass::Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir)
{
    if (bIsInitialized)
    {
        Shutdown();
    }

    Device = InDevice;
    ShaderDataDir = InShaderDataDir;

    ComputeShader = FShaderLibrary::Get().LoadShader(
        Device, ShaderDataDir, TXT("TonemapSponza_cs.sblob"), nvrhi::ShaderType::Compute);
    if (!ComputeShader)
    {
        HLVM_LOG(LogRenderer, err, TXT("FToneMappingPass: Failed to load compute shader"));
        return false;
    }

    // Create binding layout
    nvrhi::BindingLayoutDesc LayoutDesc;
    LayoutDesc.setVisibility(nvrhi::ShaderType::Compute);

    nvrhi::VulkanBindingOffsets Offsets;
    Offsets.setConstantBufferOffset(0)
           .setShaderResourceOffset(0)
           .setSamplerOffset(0)
           .setUnorderedAccessViewOffset(0);
    LayoutDesc.setBindingOffsets(Offsets);

    LayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(256));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(2));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(3));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(128));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_UAV(384));

    // Create linear sampler
    nvrhi::SamplerDesc SamplerDesc;
    SamplerDesc.setAddressU(nvrhi::SamplerAddressMode::ClampToEdge)
               .setAddressV(nvrhi::SamplerAddressMode::ClampToEdge)
               .setMinFilter(true)
               .setMagFilter(true)
               .setMipFilter(true);
    LinearSampler = Device->createSampler(SamplerDesc);

    BindingLayout = Device->createBindingLayout(LayoutDesc);
    if (!BindingLayout)
    {
        HLVM_LOG(LogRenderer, err, TXT("FToneMappingPass: Failed to create binding layout"));
        return false;
    }

    // Create compute pipeline
    nvrhi::ComputePipelineDesc PipelineDesc;
    PipelineDesc.setComputeShader(ComputeShader);
    PipelineDesc.addBindingLayout(BindingLayout);
    Pipeline = Device->createComputePipeline(PipelineDesc);
    if (!Pipeline)
    {
        HLVM_LOG(LogRenderer, err, TXT("FToneMappingPass: Failed to create compute pipeline"));
        return false;
    }

    // Create constant buffer
    nvrhi::BufferDesc CBDesc;
    CBDesc.setByteSize(sizeof(FConstants))
          .setIsConstantBuffer(true)
          .setInitialState(nvrhi::ResourceStates::ConstantBuffer)
          .setKeepInitialState(true);
    ConstantBuffer = Device->createBuffer(CBDesc);
    if (!ConstantBuffer)
    {
        HLVM_LOG(LogRenderer, err, TXT("FToneMappingPass: Failed to create constant buffer"));
        return false;
    }

    bIsInitialized = true;
    HLVM_LOG(LogRenderer, info, TXT("FToneMappingPass initialized successfully"));
    return true;
}

void FToneMappingPass::Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FConstants& Constants)
{
    if (!bIsInitialized || !Pipeline || !CmdList)
    {
        return;
    }

    // Write constants
    CmdList->writeBuffer(ConstantBuffer, &Constants, sizeof(FConstants));

    // Create per-dispatch binding set
    nvrhi::BindingSetDesc SetDesc;
    SetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(256, ConstantBuffer));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, Desc.HDRInputTexture));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, Desc.BloomTexture));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(2, Desc.SSRTexture));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(3, Desc.AdaptedLuminanceTexture));
    SetDesc.addItem(nvrhi::BindingSetItem::Sampler(128, LinearSampler));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_UAV(384, Desc.SDROutputTexture));

    nvrhi::BindingSetHandle BindingSet = Device->createBindingSet(SetDesc, BindingLayout);
    if (!BindingSet)
    {
        HLVM_LOG(LogRenderer, err, TXT("FToneMappingPass: Failed to create binding set"));
        return;
    }

    // Dispatch
    nvrhi::ComputeState State;
    State.setPipeline(Pipeline);
    State.addBindingSet(BindingSet);
    CmdList->setComputeState(State);

    uint32_t DispatchX = (Desc.Width + 7) / 8;
    uint32_t DispatchY = (Desc.Height + 7) / 8;
    CmdList->dispatch(DispatchX, DispatchY, 1);
}

void FToneMappingPass::Shutdown()
{
    ComputeShader = nullptr;
    BindingLayout = nullptr;
    Pipeline = nullptr;
    ConstantBuffer = nullptr;
    LinearSampler = nullptr;
    Device = nullptr;
    bIsInitialized = false;
}
