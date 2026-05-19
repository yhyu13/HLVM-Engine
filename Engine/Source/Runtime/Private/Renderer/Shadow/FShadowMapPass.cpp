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

#include "Renderer/Shadow/FShadowMapPass.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
#include "Core/Log.h"
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <vector>

DECLARE_LOG_CATEGORY(LogRenderer)

bool FShadowMapPass::Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir, const FDesc& Desc)
{
    if (bIsInitialized)
    {
        Shutdown();
    }

    Device = InDevice;
    ShadowMapSize = Desc.ShadowMapSize;

    auto VSPath = FPath::Combine(InShaderDataDir, TXT("ShadowVS.sblob"));
    std::ifstream VSFile(VSPath.string(), std::ios::ate | std::ios::binary);
    if (!VSFile.is_open())
    {
        HLVM_LOG(LogRenderer, err, TXT("FShadowMapPass: Failed to open vertex shader"));
        return false;
    }
    size_t VSBytes = static_cast<size_t>(VSFile.tellg());
    std::vector<uint8_t> VSData(VSBytes);
    VSFile.seekg(0);
    VSFile.read(reinterpret_cast<char*>(VSData.data()), static_cast<std::streamsize>(VSBytes));
    VSFile.close();

    const void* VSBinary = nullptr;
    size_t VSBinarySize = 0;
    if (!ShaderMake::FindPermutationInBlob(VSData.data(), VSData.size(), nullptr, 0, &VSBinary, &VSBinarySize))
    {
        HLVM_LOG(LogRenderer, err, TXT("FShadowMapPass: Failed to extract vertex shader SPIR-V from blob"));
        return false;
    }

    nvrhi::ShaderDesc VSDesc;
    VSDesc.setShaderType(nvrhi::ShaderType::Vertex);
    ShadowVS = Device->createShader(VSDesc, VSBinary, VSBinarySize);
    if (!ShadowVS)
    {
        HLVM_LOG(LogRenderer, err, TXT("FShadowMapPass: Failed to create vertex shader"));
        return false;
    }

    // Create shadow map texture
    nvrhi::TextureDesc TexDesc;
    TexDesc.dimension = nvrhi::TextureDimension::Texture2D;
    TexDesc.width = ShadowMapSize;
    TexDesc.height = ShadowMapSize;
    TexDesc.format = Desc.DepthFormat;
    TexDesc.isRenderTarget = true;
    TexDesc.isUAV = false;
    TexDesc.isTypeless = true;
    TexDesc.initialState = nvrhi::ResourceStates::DepthWrite;
    TexDesc.keepInitialState = true;
    TexDesc.debugName = "ShadowMap";
    ShadowMapTexture = Device->createTexture(TexDesc);
    if (!ShadowMapTexture)
    {
        HLVM_LOG(LogRenderer, err, TXT("FShadowMapPass: Failed to create shadow map texture"));
        return false;
    }

    // Create shadow map framebuffer (depth-only)
    nvrhi::FramebufferDesc FBDesc;
    nvrhi::FramebufferAttachment DepthAttach;
    DepthAttach.setTexture(ShadowMapTexture);
    FBDesc.setDepthAttachment(DepthAttach);
    ShadowMapFramebuffer = Device->createFramebuffer(FBDesc);
    if (!ShadowMapFramebuffer)
    {
        HLVM_LOG(LogRenderer, err, TXT("FShadowMapPass: Failed to create shadow map framebuffer"));
        return false;
    }

    // Create shadow constants buffer (128 bytes: 2x float4x4)
    nvrhi::BufferDesc CBDesc;
    CBDesc.setByteSize(sizeof(FShadowConstants))
        .setIsConstantBuffer(true)
        .setInitialState(nvrhi::ResourceStates::ConstantBuffer)
        .setKeepInitialState(true);
    ShadowConstantsBuffer = Device->createBuffer(CBDesc);
    if (!ShadowConstantsBuffer)
    {
        HLVM_LOG(LogRenderer, err, TXT("FShadowMapPass: Failed to create shadow constants buffer"));
        return false;
    }

    // Create shadow sampler (clamp-to-border, white border)
    nvrhi::SamplerDesc SamplerDesc;
    SamplerDesc.setAddressU(nvrhi::SamplerAddressMode::ClampToBorder)
        .setAddressV(nvrhi::SamplerAddressMode::ClampToBorder)
        .setAddressW(nvrhi::SamplerAddressMode::ClampToBorder)
        .setBorderColor(nvrhi::Color(1.0f, 1.0f, 1.0f, 1.0f))
        .setMinFilter(true)
        .setMagFilter(true)
        .setMipFilter(false);
    ShadowSampler = Device->createSampler(SamplerDesc);
    if (!ShadowSampler)
    {
        HLVM_LOG(LogRenderer, err, TXT("FShadowMapPass: Failed to create shadow sampler"));
        return false;
    }

    // Create binding layout
    nvrhi::BindingLayoutDesc LayoutDesc;
    LayoutDesc.setVisibility(nvrhi::ShaderType::Vertex);

    nvrhi::VulkanBindingOffsets Offsets;
    Offsets.setConstantBufferOffset(0)
           .setShaderResourceOffset(0)
           .setSamplerOffset(0)
           .setUnorderedAccessViewOffset(0);
    LayoutDesc.setBindingOffsets(Offsets);

    LayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(256));
    ShadowBindingLayout = Device->createBindingLayout(LayoutDesc);
    if (!ShadowBindingLayout)
    {
        HLVM_LOG(LogRenderer, err, TXT("FShadowMapPass: Failed to create binding layout"));
        return false;
    }

    // Create shadow pipeline
    nvrhi::GraphicsPipelineDesc PipelineDesc;
    PipelineDesc.setVertexShader(ShadowVS)
        .addBindingLayout(ShadowBindingLayout)
        .setPrimType(nvrhi::PrimitiveType::TriangleList);
    PipelineDesc.renderState.depthStencilState
        .setDepthTestEnable(true)
        .setDepthWriteEnable(true)
        .setDepthFunc(nvrhi::ComparisonFunc::Less);

    ShadowPipeline = Device->createGraphicsPipeline(PipelineDesc, ShadowMapFramebuffer->getFramebufferInfo());
    if (!ShadowPipeline)
    {
        HLVM_LOG(LogRenderer, err, TXT("FShadowMapPass: Failed to create shadow pipeline"));
        return false;
    }

    bIsInitialized = true;
    HLVM_LOG(LogRenderer, info, TXT("FShadowMapPass initialized successfully"));
    return true;
}

void FShadowMapPass::Render(nvrhi::ICommandList* CmdList, const FRenderDesc& Desc)
{
    if (!bIsInitialized || !CmdList)
    {
        return;
    }

    // Clear shadow map
    CmdList->clearDepthStencilTexture(ShadowMapTexture, nvrhi::AllSubresources, true, 1.0f, false, 0);

    // Draw all meshes
    for (uint32_t MeshIdx = 0; MeshIdx < Desc.MeshDrawItemCount; ++MeshIdx)
    {
        const auto& DrawData = Desc.MeshDrawItems[MeshIdx];

        // Upload shadow constants
        FShadowConstants Constants;
        memcpy(Constants.ModelMatrix, glm::value_ptr(Desc.ModelMatrix), 64);
        memcpy(Constants.LightViewProj, glm::value_ptr(Desc.LightViewProj), 64);
        CmdList->writeBuffer(ShadowConstantsBuffer, &Constants, sizeof(Constants));

        nvrhi::BindingSetDesc SetDesc;
        SetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(256, ShadowConstantsBuffer));
        nvrhi::BindingSetHandle BindingSet = Device->createBindingSet(SetDesc, ShadowBindingLayout);

        nvrhi::GraphicsState State;
        State.setPipeline(ShadowPipeline)
            .setFramebuffer(ShadowMapFramebuffer)
            .addBindingSet(BindingSet);

        nvrhi::VertexBufferBinding VBBinding;
        VBBinding.setBuffer(DrawData.VertexBuffer)
            .setSlot(0)
            .setOffset(0);
        State.addVertexBuffer(VBBinding);

        nvrhi::IndexBufferBinding IBBinding;
        IBBinding.setBuffer(DrawData.IndexBuffer)
            .setOffset(0)
            .setFormat(nvrhi::Format::R32_UINT);
        State.setIndexBuffer(IBBinding);

        nvrhi::Viewport shadowViewport(0.f, float(ShadowMapSize), 0.f, float(ShadowMapSize), 0.0f, 1.0f);
        State.viewport.addViewportAndScissorRect(shadowViewport);

        CmdList->setGraphicsState(State);

        nvrhi::DrawArguments DrawArgs;
        DrawArgs.vertexCount = DrawData.IndexCount;
        CmdList->drawIndexed(DrawArgs);
    }

    // Transition shadow map to ShaderResource for lighting pass
    CmdList->setTextureState(ShadowMapTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
}

void FShadowMapPass::Shutdown()
{
    ShadowVS = nullptr;
    ShadowBindingLayout = nullptr;
    ShadowPipeline = nullptr;
    ShadowMapTexture = nullptr;
    ShadowMapFramebuffer = nullptr;
    ShadowConstantsBuffer = nullptr;
    ShadowSampler = nullptr;
    Device = nullptr;
    bIsInitialized = false;
}
