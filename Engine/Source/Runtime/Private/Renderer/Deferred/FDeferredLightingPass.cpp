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

FDeferredLightingPass::FDeferredLightingPass() = default;

FDeferredLightingPass::~FDeferredLightingPass()
{
    Cleanup();
}

void FDeferredLightingPass::Cleanup()
{
    ComputeShader = nullptr;
    BindingLayout = nullptr;
    BindingSet = nullptr;
    Pipeline = nullptr;
    OutputTexture = nullptr;
    TempOutputTexture = nullptr;
    bIsInitialized = false;
}

bool FDeferredLightingPass::Initialize(
    nvrhi::IDevice* InDevice,
    FGBufferTextures* InGBuffer,
    nvrhi::TextureHandle InOutputTexture)
{
    if (bIsInitialized)
    {
        Cleanup();
    }

    Device = InDevice;
    GBuffer = InGBuffer;
    OutputTexture = InOutputTexture;

    // Read compute shader from Donut's pre-compiled SPIR-V
    const auto DataDir = FString::Format(
        TXT("{}/../../Test/{}_Data"),
        *GExecutablePath,
        *GExecutableName);

    auto CSPath = FPath::Combine(DataDir, TXT("deferred_lighting_cs.sblob"));
    std::ifstream CSFile(CSPath.string(), std::ios::ate | std::ios::binary);
    if (!CSFile.is_open())
    {
        HLVM_LOG(LogRenderer, err, TXT("Failed to open compute shader"));
        return false;
    }
    size_t CSBytes = static_cast<size_t>(CSFile.tellg());
    std::vector<uint8_t> CSData(CSBytes);
    CSFile.seekg(0);
    CSFile.read(reinterpret_cast<char*>(CSData.data()), static_cast<std::streamsize>(CSBytes));
CSFile.close();

    // Extract SPIR-V from blob (or pass through if raw SPIR-V)
    // deferred_lighting_cs.bin is raw SPIR-V, so FindPermutationInBlob with nullptr/0 will return it directly
    const void* CSBinary = nullptr;
    size_t CSBinarySize = 0;
    if (!ShaderMake::FindPermutationInBlob(CSData.data(), CSData.size(), nullptr, 0, &CSBinary, &CSBinarySize))
    {
        HLVM_LOG(LogRenderer, err, TXT("Failed to extract compute shader SPIR-V from blob"));
        return false;
    }

    // Create compute shader
    nvrhi::ShaderDesc CSDsc;
    CSDsc.setShaderType(nvrhi::ShaderType::Compute);
    ComputeShader = Device->createShader(CSDsc, CSBinary, CSBinarySize);
    if (!ComputeShader)
    {
        HLVM_LOG(LogRenderer, err, TXT("Failed to create compute shader"));
        return false;
    }

    // Create binding layout
    if (!CreateBindingLayout())
    {
        HLVM_LOG(LogRenderer, err, TXT("Failed to create binding layout"));
        return false;
    }

    // Create compute pipeline
    nvrhi::ComputePipelineDesc PipelineDesc;
    PipelineDesc.setComputeShader(ComputeShader);
    PipelineDesc.addBindingLayout(BindingLayout);
    Pipeline = Device->createComputePipeline(PipelineDesc);
    if (!Pipeline)
    {
        HLVM_LOG(LogRenderer, err, TXT("Failed to create compute pipeline"));
        return false;
    }

    bIsInitialized = true;
    HLVM_LOG(LogRenderer, info, TXT("FDeferredLightingPass initialized successfully"));
    return true;
}

bool FDeferredLightingPass::CreateBindingLayout()
{
    nvrhi::BindingLayoutDesc LayoutDesc;
    LayoutDesc.setVisibility(nvrhi::ShaderType::Compute);

    // Set binding offsets to 0 to match GLSL binding numbers
    nvrhi::VulkanBindingOffsets Offsets;
    Offsets.setConstantBufferOffset(0)
           .setShaderResourceOffset(0)
           .setSamplerOffset(0)
           .setUnorderedAccessViewOffset(0);
    LayoutDesc.setBindingOffsets(Offsets);

    // b0: LightingConstants (CBV) - bRegShift 256 → SPIR-V binding 256
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(256));
    // t0-t2: GBuffer textures (SRV) - tRegShift 0 → SPIR-V bindings 0-2
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(2));
    // u0: Output (UAV) - uRegShift 384 → SPIR-V binding 384
    BindingLayout = Device->createBindingLayout(LayoutDesc);
    return BindingLayout != nullptr;
}

bool FDeferredLightingPass::UpdateBindingSet()
{
    if (!Device || !BindingLayout || !GBuffer || !OutputTexture || !LightingConstants)
    {
        return false;
    }

    // Create constant buffer for lighting constants
    nvrhi::BufferHandle LightingBuffer = Device->createBuffer(
        nvrhi::BufferDesc()
            .setByteSize(sizeof(FLightingConstants))
            .setIsConstantBuffer(true)
            .setInitialState(nvrhi::ResourceStates::ConstantBuffer)
            .setKeepInitialState(true));

    // Write lighting constants to buffer
    nvrhi::CommandListHandle CmdList = Device->createCommandList();
    CmdList->open();
    CmdList->writeBuffer(LightingBuffer, LightingConstants, sizeof(FLightingConstants), 0);
    CmdList->close();
    Device->executeCommandList(CmdList);

    nvrhi::BindingSetDesc SetDesc;
    SetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(256, LightingBuffer));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, GBuffer->GetDepthTexture()));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, GBuffer->GetTexture(EGBufferTexture::GBufferDiffuse)));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(2, GBuffer->GetTexture(EGBufferTexture::GBufferSpecular)));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(3, GBuffer->GetTexture(EGBufferTexture::GBufferNormals)));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(4, GBuffer->GetTexture(EGBufferTexture::GBufferEmissive)));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_UAV(384, OutputTexture));

    BindingSet = Device->createBindingSet(SetDesc, BindingLayout);
    return BindingSet != nullptr;
}

void FDeferredLightingPass::SetLightingConstants(const FLightingConstants* InLightingConstants)
{
    LightingConstants = InLightingConstants;
    // Update binding set with new constants
    UpdateBindingSet();
}

void FDeferredLightingPass::Render(nvrhi::ICommandList* CommandList)
{
    if (!bIsInitialized || !Pipeline || !GBuffer)
    {
        return;
    }

    // Update binding set with current lighting constants
    if (!UpdateBindingSet())
    {
        HLVM_LOG(LogRenderer, err, TXT("Failed to update binding set"));
        return;
    }

    // Set up compute state
    nvrhi::ComputeState State;
    State.setPipeline(Pipeline);
    State.addBindingSet(BindingSet);
    CommandList->setComputeState(State);

    // Calculate dispatch dimensions based on GBuffer size
    uint32_t Width = GBuffer->GetWidth();
    uint32_t Height = GBuffer->GetHeight();

    // Dispatch with 8x8 compute tile size (typical for deferred lighting)
    // Round up to nearest 8
    uint32_t DispatchX = (Width + 7) / 8;
    uint32_t DispatchY = (Height + 7) / 8;

    CommandList->dispatch(DispatchX, DispatchY, 1);
}
