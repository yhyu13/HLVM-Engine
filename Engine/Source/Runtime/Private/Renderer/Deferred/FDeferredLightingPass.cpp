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
#include "Renderer/Shader/ShaderLibrary.h"
#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogRenderer)

bool FDeferredLightingPass::Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir)
{
    if (bIsInitialized)
    {
        Shutdown();
    }

    Device = InDevice;
    ShaderDataDir = InShaderDataDir;

    // Load all 4 permutations of the deferred lighting shader
    // Index: (bSSAO ? 1 : 0) | (bContactShadows ? 2 : 0)
    // All macros must be explicitly specified because the blob is a multi-permutation blob;
    // FindPermutationInBlob with numConstants==0 only works on single-permutation blobs.
    const TVector<FShaderMacro> PermutationMacros[PERMUTATION_COUNT] = {
        { FShaderMacro(TXT("ENABLE_SSAO"), TXT("0")),
          FShaderMacro(TXT("ENABLE_CONTACT_SHADOWS"), TXT("0")) }, // 0: no features
        { FShaderMacro(TXT("ENABLE_SSAO"), TXT("1")),
          FShaderMacro(TXT("ENABLE_CONTACT_SHADOWS"), TXT("0")) }, // 1: SSAO only
        { FShaderMacro(TXT("ENABLE_SSAO"), TXT("0")),
          FShaderMacro(TXT("ENABLE_CONTACT_SHADOWS"), TXT("1")) }, // 2: ContactShadows only
        { FShaderMacro(TXT("ENABLE_SSAO"), TXT("1")),
          FShaderMacro(TXT("ENABLE_CONTACT_SHADOWS"), TXT("1")) }, // 3: both
    };

    nvrhi::ShaderHandle PermutationShaders[PERMUTATION_COUNT];
    for (uint32_t i = 0; i < PERMUTATION_COUNT; ++i)
    {
        PermutationShaders[i] = FShaderLibrary::Get().LoadShader(
            Device, ShaderDataDir, TXT("SponzaDeferredLighting_cs.sblob"),
            nvrhi::ShaderType::Compute, PermutationMacros[i]);
        if (!PermutationShaders[i])
        {
            HLVM_LOG(LogRenderer, err, TXT("FDeferredLightingPass: Failed to load compute shader permutation {}"), i);
            return false;
        }
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

    // Create compute pipelines for all permutations
    for (uint32_t i = 0; i < PERMUTATION_COUNT; ++i)
    {
        nvrhi::ComputePipelineDesc PipelineDesc;
        PipelineDesc.setComputeShader(PermutationShaders[i]);
        PipelineDesc.addBindingLayout(BindingLayout);
        Pipelines[i] = Device->createComputePipeline(PipelineDesc);
        if (!Pipelines[i])
        {
            HLVM_LOG(LogRenderer, err, TXT("FDeferredLightingPass: Failed to create compute pipeline for permutation {}"), i);
            return false;
        }
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
    uint32_t PermIdx = GetPermutationIndex(Desc.bEnableSSAO, Desc.bEnableContactShadows);
    if (!bIsInitialized || !Pipelines[PermIdx] || !CmdList)
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
    State.setPipeline(Pipelines[PermIdx]);
    State.addBindingSet(BindingSet);
    CmdList->setComputeState(State);

    uint32_t DispatchX = (Desc.Width + 7) / 8;
    uint32_t DispatchY = (Desc.Height + 7) / 8;
    CmdList->dispatch(DispatchX, DispatchY, 1);
}

void FDeferredLightingPass::Shutdown()
{
    for (uint32_t i = 0; i < PERMUTATION_COUNT; ++i)
    {
        Pipelines[i] = nullptr;
    }
    BindingLayout = nullptr;
    ConstantBuffer = nullptr;
    Device = nullptr;
    bIsInitialized = false;
}
