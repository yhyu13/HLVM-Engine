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

#include "Renderer/Deferred/FDeferredLightingPass.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
#include "Core/Log.h"
#include <fstream>
#include <vector>

DECLARE_LOG_CATEGORY(LogRenderer)

bool FDeferredLightingPass::Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir)
{
    if (bIsInitialized)
    {
        Shutdown();
    }

    Device = InDevice;
    ShaderDataDir = InShaderDataDir;

    auto CSPath = FPath::Combine(ShaderDataDir, TXT("SponzaDeferredLighting_cs.sblob"));
    std::ifstream CSFile(CSPath.string(), std::ios::ate | std::ios::binary);
    if (!CSFile.is_open())
    {
        HLVM_LOG(LogRenderer, err, TXT("FDeferredLightingPass: Failed to open compute shader"));
        return false;
    }
    size_t CSBytes = static_cast<size_t>(CSFile.tellg());
    std::vector<uint8_t> CSData(CSBytes);
    CSFile.seekg(0);
    CSFile.read(reinterpret_cast<char*>(CSData.data()), static_cast<std::streamsize>(CSBytes));
    CSFile.close();

    const void* CSBinary = nullptr;
    size_t CSBinarySize = 0;
    if (!ShaderMake::FindPermutationInBlob(CSData.data(), CSData.size(), nullptr, 0, &CSBinary, &CSBinarySize))
    {
        HLVM_LOG(LogRenderer, err, TXT("FDeferredLightingPass: Failed to extract compute shader SPIR-V from blob"));
        return false;
    }

    nvrhi::ShaderDesc CSDesc;
    CSDesc.setShaderType(nvrhi::ShaderType::Compute);
    ComputeShader = Device->createShader(CSDesc, CSBinary, CSBinarySize);
    if (!ComputeShader)
    {
        HLVM_LOG(LogRenderer, err, TXT("FDeferredLightingPass: Failed to create compute shader"));
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

    // b0: LightingConstants (CBV) - bRegShift 256 -> SPIR-V binding 256
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(256));
    // t0-t7: GBuffer textures + contact shadows (SRV) - tRegShift 0 -> SPIR-V bindings 0-7
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(2));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(3));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(4));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(5));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(6));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(7));
    // s1: ShadowSampler - sRegShift 128 -> SPIR-V binding 129
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(129));
    // u0: Output (UAV) - uRegShift 384 -> SPIR-V binding 384
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_UAV(384));

    BindingLayout = Device->createBindingLayout(LayoutDesc);
    if (!BindingLayout)
    {
        HLVM_LOG(LogRenderer, err, TXT("FDeferredLightingPass: Failed to create binding layout"));
        return false;
    }

    // Create compute pipeline
    nvrhi::ComputePipelineDesc PipelineDesc;
    PipelineDesc.setComputeShader(ComputeShader);
    PipelineDesc.addBindingLayout(BindingLayout);
    Pipeline = Device->createComputePipeline(PipelineDesc);
    if (!Pipeline)
    {
        HLVM_LOG(LogRenderer, err, TXT("FDeferredLightingPass: Failed to create compute pipeline"));
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
        HLVM_LOG(LogRenderer, err, TXT("FDeferredLightingPass: Failed to create constant buffer"));
        return false;
    }

    bIsInitialized = true;
    HLVM_LOG(LogRenderer, info, TXT("FDeferredLightingPass initialized successfully"));
    return true;
}

void FDeferredLightingPass::Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FConstants& Constants)
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
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, Desc.GBufferDiffuse));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, Desc.GBufferMaterial));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(2, Desc.GBufferNormals));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(3, Desc.GBufferEmissive));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(4, Desc.GBufferDepth));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(5, Desc.ShadowMap));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(6, Desc.SSAOTexture));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(7, Desc.ContactShadowTexture));
    SetDesc.addItem(nvrhi::BindingSetItem::Sampler(129, Desc.ShadowSampler));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_UAV(384, Desc.HDROutputTexture));

    nvrhi::BindingSetHandle BindingSet = Device->createBindingSet(SetDesc, BindingLayout);
    if (!BindingSet)
    {
        HLVM_LOG(LogRenderer, err, TXT("FDeferredLightingPass: Failed to create binding set"));
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

void FDeferredLightingPass::Shutdown()
{
    ComputeShader = nullptr;
    BindingLayout = nullptr;
    Pipeline = nullptr;
    ConstantBuffer = nullptr;
    Device = nullptr;
    bIsInitialized = false;
}
