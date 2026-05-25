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
#include "Renderer/Shader/ShaderLibrary.h"
#include "Renderer/Mesh/IMesh.h"
#include "Core/Log.h"
#include <glm/gtc/type_ptr.hpp>

DECLARE_LOG_CATEGORY(LogRenderer)

bool FShadowMapPass::Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir, const FDesc& Desc)
{
    if (bIsInitialized)
    {
        Shutdown();
    }

    Device = InDevice;
    ShadowMapSize = Desc.ShadowMapSize;

    ShadowVS = FShaderLibrary::Get().LoadShader(
        Device, InShaderDataDir, TXT("ShadowVS.sblob"), nvrhi::ShaderType::Vertex);
    if (!ShadowVS)
    {
        HLVM_LOG(LogRenderer, err, TXT("FShadowMapPass: Failed to load vertex shader"));
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

    // Create input layout matching the shadow vertex shader
    nvrhi::VertexAttributeDesc Attrs[4];
    Attrs[0].setName("POSITION")
        .setFormat(nvrhi::Format::RGB32_FLOAT)
        .setOffset(offsetof(FVertex, Position))
        .setElementStride(sizeof(FVertex));
    Attrs[1].setName("NORMAL")
        .setFormat(nvrhi::Format::RGB32_FLOAT)
        .setOffset(offsetof(FVertex, Normal))
        .setElementStride(sizeof(FVertex));
    Attrs[2].setName("TEXCOORD0")
        .setFormat(nvrhi::Format::RG32_FLOAT)
        .setOffset(offsetof(FVertex, UV))
        .setElementStride(sizeof(FVertex));
    Attrs[3].setName("TANGENT")
        .setFormat(nvrhi::Format::RGB32_FLOAT)
        .setOffset(offsetof(FVertex, Tangent))
        .setElementStride(sizeof(FVertex));

    ShadowInputLayout = Device->createInputLayout(Attrs, 4, ShadowVS);
    if (!ShadowInputLayout)
    {
        HLVM_LOG(LogRenderer, err, TXT("FShadowMapPass: Failed to create input layout"));
        return false;
    }

    // Create shadow pipeline
    nvrhi::GraphicsPipelineDesc PipelineDesc;
    PipelineDesc.setVertexShader(ShadowVS)
        .setInputLayout(ShadowInputLayout)
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

    // Load instanced vertex shader (optional — if not present, instancing is unavailable)
    ShadowInstancedVS = FShaderLibrary::Get().LoadShader(
        Device, InShaderDataDir, TXT("ShadowInstancedVS.sblob"), nvrhi::ShaderType::Vertex);
    if (ShadowInstancedVS)
    {
        // Create instanced binding layout (adds StructuredBuffer_SRV at slot 10)
        nvrhi::BindingLayoutDesc InstancedLayoutDesc;
        InstancedLayoutDesc.setVisibility(nvrhi::ShaderType::Vertex);

        nvrhi::VulkanBindingOffsets instancedOffsets;
        instancedOffsets.setConstantBufferOffset(0)
                        .setShaderResourceOffset(0)
                        .setSamplerOffset(0)
                        .setUnorderedAccessViewOffset(0);
        InstancedLayoutDesc.setBindingOffsets(instancedOffsets);

        InstancedLayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(256));
        InstancedLayoutDesc.addItem(nvrhi::BindingLayoutItem::StructuredBuffer_SRV(10));

        ShadowInstancedBindingLayout = Device->createBindingLayout(InstancedLayoutDesc);
        if (ShadowInstancedBindingLayout)
        {
            // Create instanced shadow pipeline
            nvrhi::GraphicsPipelineDesc InstancedPipelineDesc;
            InstancedPipelineDesc.setVertexShader(ShadowInstancedVS)
                .setInputLayout(ShadowInputLayout)
                .addBindingLayout(ShadowInstancedBindingLayout)
                .setPrimType(nvrhi::PrimitiveType::TriangleList);
            InstancedPipelineDesc.renderState.depthStencilState
                .setDepthTestEnable(true)
                .setDepthWriteEnable(true)
                .setDepthFunc(nvrhi::ComparisonFunc::Less);

            ShadowInstancedPipeline = Device->createGraphicsPipeline(InstancedPipelineDesc, ShadowMapFramebuffer->getFramebufferInfo());
        }
    }
    else
    {
        HLVM_LOG(LogRenderer, warn, TXT("FShadowMapPass: Instanced vertex shader not found — instancing disabled"));
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

void FShadowMapPass::RenderInstanced(nvrhi::ICommandList* CmdList, const FInstancedRenderDesc& Desc)
{
    if (!bIsInitialized || !CmdList)
    {
        return;
    }

    if (!ShadowInstancedPipeline)
    {
        HLVM_LOG(LogRenderer, err, TXT("FShadowMapPass::RenderInstanced: Instanced pipeline not available"));
        return;
    }

    // Clear shadow map
    CmdList->clearDepthStencilTexture(ShadowMapTexture, nvrhi::AllSubresources, true, 1.0f, false, 0);

    // Upload shadow constants (LightViewProj only, ModelMatrix unused by instanced VS)
    FShadowConstants Constants;
    memset(&Constants, 0, sizeof(Constants));
    memcpy(Constants.LightViewProj, glm::value_ptr(Desc.LightViewProj), 64);
    CmdList->writeBuffer(ShadowConstantsBuffer, &Constants, sizeof(Constants));

    // Draw all instanced items
    for (uint32_t i = 0; i < Desc.InstancedItemCount; ++i)
    {
        const auto& Item = Desc.InstancedItems[i];

        nvrhi::BindingSetDesc SetDesc;
        SetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(256, ShadowConstantsBuffer));
        SetDesc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(10, Item.InstanceBuffer));
        nvrhi::BindingSetHandle BindingSet = Device->createBindingSet(SetDesc, ShadowInstancedBindingLayout);

        nvrhi::GraphicsState State;
        State.setPipeline(ShadowInstancedPipeline)
            .setFramebuffer(ShadowMapFramebuffer)
            .addBindingSet(BindingSet);

        nvrhi::VertexBufferBinding VBBinding;
        VBBinding.setBuffer(Item.VertexBuffer)
            .setSlot(0)
            .setOffset(0);
        State.addVertexBuffer(VBBinding);

        nvrhi::IndexBufferBinding IBBinding;
        IBBinding.setBuffer(Item.IndexBuffer)
            .setOffset(0)
            .setFormat(nvrhi::Format::R32_UINT);
        State.setIndexBuffer(IBBinding);

        nvrhi::Viewport shadowViewport(0.f, float(ShadowMapSize), 0.f, float(ShadowMapSize), 0.0f, 1.0f);
        State.viewport.addViewportAndScissorRect(shadowViewport);

        CmdList->setGraphicsState(State);

        nvrhi::DrawArguments DrawArgs;
        DrawArgs.vertexCount = Item.IndexCount;
        DrawArgs.instanceCount = Item.InstanceCount;
        DrawArgs.startInstanceLocation = 0;
        CmdList->drawIndexed(DrawArgs);
    }

    // Transition shadow map to ShaderResource for lighting pass
    CmdList->setTextureState(ShadowMapTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
}

void FShadowMapPass::Shutdown()
{
    ShadowVS = nullptr;
    ShadowInputLayout = nullptr;
    ShadowBindingLayout = nullptr;
    ShadowPipeline = nullptr;
    ShadowInstancedVS = nullptr;
    ShadowInstancedBindingLayout = nullptr;
    ShadowInstancedPipeline = nullptr;
    ShadowMapTexture = nullptr;
    ShadowMapFramebuffer = nullptr;
    ShadowConstantsBuffer = nullptr;
    ShadowSampler = nullptr;
    Device = nullptr;
    bIsInitialized = false;
}
