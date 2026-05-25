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

#include "Renderer/Deferred/FGBufferFillPass.h"
#include "Renderer/Shader/ShaderLibrary.h"
#include "Core/Log.h"
#include "Renderer/Mesh/StaticMesh.h"
#include <nvrhi/utils.h>

DECLARE_LOG_CATEGORY(LogRenderer)

bool FGBufferFillPass::Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir, const FDesc& Desc)
{
    if (bIsInitialized)
    {
        Shutdown();
    }

    Device = InDevice;

    GBufferVS = FShaderLibrary::Get().LoadShader(
        Device, InShaderDataDir, TXT("GBufferSponzaVS.sblob"), nvrhi::ShaderType::Vertex);
    if (!GBufferVS)
    {
        HLVM_LOG(LogRenderer, err, TXT("FGBufferFillPass: Failed to load vertex shader"));
        return false;
    }

    GBufferPS = FShaderLibrary::Get().LoadShader(
        Device, InShaderDataDir, TXT("GBufferSponzaPS.sblob"), nvrhi::ShaderType::Pixel);
    if (!GBufferPS)
    {
        HLVM_LOG(LogRenderer, err, TXT("FGBufferFillPass: Failed to load pixel shader"));
        return false;
    }

    // Create input layout
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

    GBufferInputLayout = Device->createInputLayout(Attrs, 4, GBufferVS);
    if (!GBufferInputLayout)
    {
        HLVM_LOG(LogRenderer, err, TXT("FGBufferFillPass: Failed to create input layout"));
        return false;
    }

    // Create binding layout
    nvrhi::BindingLayoutDesc LayoutDesc;
    LayoutDesc.setVisibility(nvrhi::ShaderType::Vertex | nvrhi::ShaderType::Pixel);

    nvrhi::VulkanBindingOffsets offsets;
    offsets.setConstantBufferOffset(0)
           .setShaderResourceOffset(0)
           .setSamplerOffset(0)
           .setUnorderedAccessViewOffset(0);
    LayoutDesc.setBindingOffsets(offsets);

    LayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(256));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(257));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(2));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(3));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(4));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(128));

    GBufferBindingLayout = Device->createBindingLayout(LayoutDesc);
    if (!GBufferBindingLayout)
    {
        HLVM_LOG(LogRenderer, err, TXT("FGBufferFillPass: Failed to create binding layout"));
        return false;
    }

    // Create constant buffers
    nvrhi::BufferDesc CBDesc;
    CBDesc.setByteSize(sizeof(FViewConstants))
        .setIsConstantBuffer(true)
        .setInitialState(nvrhi::ResourceStates::ConstantBuffer)
        .setKeepInitialState(true);
    ViewConstantsBuffer = Device->createBuffer(CBDesc);
    if (!ViewConstantsBuffer)
    {
        HLVM_LOG(LogRenderer, err, TXT("FGBufferFillPass: Failed to create view constants buffer"));
        return false;
    }

    nvrhi::BufferDesc MatCBDesc;
    MatCBDesc.setByteSize(sizeof(FMaterialConstants))
        .setIsConstantBuffer(true)
        .setInitialState(nvrhi::ResourceStates::ConstantBuffer)
        .setKeepInitialState(true);
    MaterialConstantBuffer = Device->createBuffer(MatCBDesc);
    if (!MaterialConstantBuffer)
    {
        HLVM_LOG(LogRenderer, err, TXT("FGBufferFillPass: Failed to create material constants buffer"));
        return false;
    }

    // Create sampler
    nvrhi::SamplerDesc SamplerDesc;
    SamplerDesc.setAddressU(nvrhi::SamplerAddressMode::Repeat)
        .setAddressV(nvrhi::SamplerAddressMode::Repeat)
        .setAddressW(nvrhi::SamplerAddressMode::Repeat)
        .setMinFilter(true)
        .setMagFilter(true)
        .setMipFilter(true);
    GBufferLinearSampler = Device->createSampler(SamplerDesc);
    if (!GBufferLinearSampler)
    {
        HLVM_LOG(LogRenderer, err, TXT("FGBufferFillPass: Failed to create sampler"));
        return false;
    }

    // Create GBuffer textures
    nvrhi::TextureDesc TexDesc;
    TexDesc.dimension = nvrhi::TextureDimension::Texture2D;
    TexDesc.width = Desc.Width;
    TexDesc.height = Desc.Height;
    TexDesc.isRenderTarget = true;
    TexDesc.isUAV = false;
    TexDesc.initialState = nvrhi::ResourceStates::RenderTarget;
    TexDesc.keepInitialState = true;

    TexDesc.format = nvrhi::Format::RGBA16_FLOAT;
    TexDesc.debugName = "GBufferDiffuse";
    GBufferDiffuseTexture = Device->createTexture(TexDesc);
    TexDesc.debugName = "GBufferSpecular";
    GBufferSpecularTexture = Device->createTexture(TexDesc);
    TexDesc.debugName = "GBufferNormals";
    GBufferNormalsTexture = Device->createTexture(TexDesc);
    TexDesc.debugName = "GBufferEmissive";
    GBufferEmissiveTexture = Device->createTexture(TexDesc);

    TexDesc.format = nvrhi::Format::D32;
    TexDesc.isTypeless = true;
    TexDesc.initialState = nvrhi::ResourceStates::DepthWrite;
    TexDesc.debugName = "GBufferDepth";
    GBufferDepthTexture = Device->createTexture(TexDesc);

    // Create framebuffer
    nvrhi::FramebufferDesc FBDesc;
    FBDesc.addColorAttachment(GBufferDiffuseTexture);
    FBDesc.addColorAttachment(GBufferSpecularTexture);
    FBDesc.addColorAttachment(GBufferNormalsTexture);
    FBDesc.addColorAttachment(GBufferEmissiveTexture);
    FBDesc.setDepthAttachment(GBufferDepthTexture);
    GBufferFramebuffer = Device->createFramebuffer(FBDesc);
    if (!GBufferFramebuffer)
    {
        HLVM_LOG(LogRenderer, err, TXT("FGBufferFillPass: Failed to create framebuffer"));
        return false;
    }

    // Create pipeline
    nvrhi::GraphicsPipelineDesc PipelineDesc;
    PipelineDesc.setVertexShader(GBufferVS)
        .setPixelShader(GBufferPS)
        .setInputLayout(GBufferInputLayout)
        .addBindingLayout(GBufferBindingLayout)
        .setPrimType(nvrhi::PrimitiveType::TriangleList);
    PipelineDesc.renderState.setRasterState(nvrhi::RasterState().setCullNone());
    PipelineDesc.renderState.depthStencilState
        .setDepthTestEnable(true)
        .setDepthWriteEnable(true)
        .setDepthFunc(nvrhi::ComparisonFunc::Less);

    GBufferPipeline = Device->createGraphicsPipeline(PipelineDesc, GBufferFramebuffer->getFramebufferInfo());
    if (!GBufferPipeline)
    {
        HLVM_LOG(LogRenderer, err, TXT("FGBufferFillPass: Failed to create pipeline"));
        return false;
    }

    // Load instanced vertex shader (optional — if not present, instancing is unavailable)
    GBufferInstancedVS = FShaderLibrary::Get().LoadShader(
        Device, InShaderDataDir, TXT("GBufferInstancedVS.sblob"), nvrhi::ShaderType::Vertex);
    if (GBufferInstancedVS)
    {
        // Create instanced binding layout (adds StructuredBuffer_SRV at slot 10)
        nvrhi::BindingLayoutDesc InstancedLayoutDesc;
        InstancedLayoutDesc.setVisibility(nvrhi::ShaderType::Vertex | nvrhi::ShaderType::Pixel);
        nvrhi::VulkanBindingOffsets instancedOffsets;
        instancedOffsets.setConstantBufferOffset(0)
                        .setShaderResourceOffset(0)
                        .setSamplerOffset(0)
                        .setUnorderedAccessViewOffset(0);
        InstancedLayoutDesc.setBindingOffsets(instancedOffsets);

        InstancedLayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(256));
        InstancedLayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(257));
        InstancedLayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));
        InstancedLayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1));
        InstancedLayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(2));
        InstancedLayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(3));
        InstancedLayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(4));
        InstancedLayoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(128));
        InstancedLayoutDesc.addItem(nvrhi::BindingLayoutItem::StructuredBuffer_SRV(10));

        GBufferInstancedBindingLayout = Device->createBindingLayout(InstancedLayoutDesc);
        if (GBufferInstancedBindingLayout)
        {
            // Create instanced pipeline (same PS, instanced VS, same input layout)
            nvrhi::GraphicsPipelineDesc InstancedPipelineDesc;
            InstancedPipelineDesc.setVertexShader(GBufferInstancedVS)
                .setPixelShader(GBufferPS)
                .setInputLayout(GBufferInputLayout)
                .addBindingLayout(GBufferInstancedBindingLayout)
                .setPrimType(nvrhi::PrimitiveType::TriangleList);
            InstancedPipelineDesc.renderState.setRasterState(nvrhi::RasterState().setCullNone());
            InstancedPipelineDesc.renderState.depthStencilState
                .setDepthTestEnable(true)
                .setDepthWriteEnable(true)
                .setDepthFunc(nvrhi::ComparisonFunc::Less);

            GBufferInstancedPipeline = Device->createGraphicsPipeline(InstancedPipelineDesc, GBufferFramebuffer->getFramebufferInfo());
        }
    }
    else
    {
        HLVM_LOG(LogRenderer, warn, TXT("FGBufferFillPass: Instanced vertex shader not found — instancing disabled"));
    }

    bIsInitialized = true;
    HLVM_LOG(LogRenderer, info, TXT("FGBufferFillPass initialized successfully"));
    return true;
}

void FGBufferFillPass::Render(nvrhi::ICommandList* CmdList, const FRenderDesc& Desc)
{
    if (!bIsInitialized || !CmdList)
    {
        return;
    }

    // Upload view constants
    CmdList->writeBuffer(ViewConstantsBuffer, &Desc.ViewConstants, sizeof(FViewConstants));

    // Clear MRTs
    nvrhi::Color clearBlack(0.f, 0.f, 0.f, 0.f);
    nvrhi::Color clearBlue(0.f, 0.f, 1.f, 1.f);
    nvrhi::Color clearNormalUp(0.5f, 1.0f, 0.5f, 1.f);
    nvrhi::utils::ClearColorAttachment(CmdList, GBufferFramebuffer, 0, clearBlue);
    nvrhi::utils::ClearColorAttachment(CmdList, GBufferFramebuffer, 1, clearBlack);
    nvrhi::utils::ClearColorAttachment(CmdList, GBufferFramebuffer, 2, clearNormalUp);
    nvrhi::utils::ClearColorAttachment(CmdList, GBufferFramebuffer, 3, clearBlack);
    nvrhi::utils::ClearDepthStencilAttachment(CmdList, GBufferFramebuffer, 1.0f, 0u);

    // Draw all meshes with per-mesh material binding
    for (uint32_t MeshIdx = 0; MeshIdx < Desc.MeshDrawItemCount; ++MeshIdx)
    {
        const auto& DrawData = Desc.MeshDrawItems[MeshIdx];

        // Upload material constants
        CmdList->writeBuffer(MaterialConstantBuffer, &DrawData.Material.Constants, sizeof(FMaterialConstants));

        // Create binding set for this mesh's textures
        nvrhi::BindingSetDesc SetDesc;
        SetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(256, ViewConstantsBuffer));
        SetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(257, MaterialConstantBuffer));
        SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, DrawData.Material.DiffuseTexture));
        SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, DrawData.Material.NormalTexture));
        SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(2, DrawData.Material.MetallicTexture));
        SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(3, DrawData.Material.RoughnessTexture));
        SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(4, DrawData.Material.AOTexture));
        SetDesc.addItem(nvrhi::BindingSetItem::Sampler(128, GBufferLinearSampler));
        nvrhi::BindingSetHandle BindingSet = Device->createBindingSet(SetDesc, GBufferBindingLayout);

        // Build graphics state
        nvrhi::GraphicsState State;
        State.setPipeline(GBufferPipeline)
            .setFramebuffer(GBufferFramebuffer)
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

        nvrhi::Viewport viewport(0.f, float(GBufferDiffuseTexture->getDesc().width), 0.f, float(GBufferDiffuseTexture->getDesc().height), 0.0f, 1.0f);
        State.viewport.addViewportAndScissorRect(viewport);

        CmdList->setGraphicsState(State);

        nvrhi::DrawArguments DrawArgs;
        DrawArgs.vertexCount = DrawData.IndexCount;
        CmdList->drawIndexed(DrawArgs);
    }

    // Transition GBuffer color textures to ShaderResource
    CmdList->setTextureState(GBufferDiffuseTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
    CmdList->setTextureState(GBufferSpecularTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
    CmdList->setTextureState(GBufferNormalsTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
    CmdList->setTextureState(GBufferEmissiveTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
    CmdList->setTextureState(GBufferDepthTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
}

void FGBufferFillPass::RenderInstanced(nvrhi::ICommandList* CmdList, const FInstancedRenderDesc& Desc)
{
    if (!bIsInitialized || !CmdList)
    {
        return;
    }

    if (!GBufferInstancedPipeline)
    {
        HLVM_LOG(LogRenderer, err, TXT("FGBufferFillPass::RenderInstanced: Instanced pipeline not available"));
        return;
    }

    // Upload view constants
    CmdList->writeBuffer(ViewConstantsBuffer, &Desc.ViewConstants, sizeof(FViewConstants));

    // Clear MRTs
    nvrhi::Color clearBlack(0.f, 0.f, 0.f, 0.f);
    nvrhi::Color clearBlue(0.f, 0.f, 1.f, 1.f);
    nvrhi::Color clearNormalUp(0.5f, 1.0f, 0.5f, 1.f);
    nvrhi::utils::ClearColorAttachment(CmdList, GBufferFramebuffer, 0, clearBlue);
    nvrhi::utils::ClearColorAttachment(CmdList, GBufferFramebuffer, 1, clearBlack);
    nvrhi::utils::ClearColorAttachment(CmdList, GBufferFramebuffer, 2, clearNormalUp);
    nvrhi::utils::ClearColorAttachment(CmdList, GBufferFramebuffer, 3, clearBlack);
    nvrhi::utils::ClearDepthStencilAttachment(CmdList, GBufferFramebuffer, 1.0f, 0u);

    // Draw all instanced items
    for (uint32_t i = 0; i < Desc.InstancedItemCount; ++i)
    {
        const auto& Item = Desc.InstancedItems[i];

        // Upload material constants
        CmdList->writeBuffer(MaterialConstantBuffer, &Item.Material.Constants, sizeof(FMaterialConstants));

        // Create binding set with instance buffer SRV
        nvrhi::BindingSetDesc SetDesc;
        SetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(256, ViewConstantsBuffer));
        SetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(257, MaterialConstantBuffer));
        SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, Item.Material.DiffuseTexture));
        SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, Item.Material.NormalTexture));
        SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(2, Item.Material.MetallicTexture));
        SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(3, Item.Material.RoughnessTexture));
        SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(4, Item.Material.AOTexture));
        SetDesc.addItem(nvrhi::BindingSetItem::Sampler(128, GBufferLinearSampler));
        SetDesc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(10, Item.InstanceBuffer));
        nvrhi::BindingSetHandle BindingSet = Device->createBindingSet(SetDesc, GBufferInstancedBindingLayout);

        // Build graphics state
        nvrhi::GraphicsState State;
        State.setPipeline(GBufferInstancedPipeline)
            .setFramebuffer(GBufferFramebuffer)
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

        nvrhi::Viewport viewport(0.f, float(GBufferDiffuseTexture->getDesc().width), 0.f, float(GBufferDiffuseTexture->getDesc().height), 0.0f, 1.0f);
        State.viewport.addViewportAndScissorRect(viewport);

        CmdList->setGraphicsState(State);

        nvrhi::DrawArguments DrawArgs;
        DrawArgs.vertexCount = Item.IndexCount;
        DrawArgs.instanceCount = Item.InstanceCount;
        DrawArgs.startInstanceLocation = 0;
        CmdList->drawIndexed(DrawArgs);
    }

    // Transition GBuffer color textures to ShaderResource
    CmdList->setTextureState(GBufferDiffuseTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
    CmdList->setTextureState(GBufferSpecularTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
    CmdList->setTextureState(GBufferNormalsTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
    CmdList->setTextureState(GBufferEmissiveTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
    CmdList->setTextureState(GBufferDepthTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
}

void FGBufferFillPass::Resize(uint32_t Width, uint32_t Height)
{
    if (!bIsInitialized || !Device)
    {
        return;
    }

    // Release old textures and framebuffer
    GBufferDiffuseTexture = nullptr;
    GBufferSpecularTexture = nullptr;
    GBufferNormalsTexture = nullptr;
    GBufferEmissiveTexture = nullptr;
    GBufferDepthTexture = nullptr;
    GBufferFramebuffer = nullptr;
    GBufferPipeline = nullptr;
    GBufferInstancedPipeline = nullptr;

    // Recreate GBuffer textures
    nvrhi::TextureDesc TexDesc;
    TexDesc.dimension = nvrhi::TextureDimension::Texture2D;
    TexDesc.width = Width;
    TexDesc.height = Height;
    TexDesc.isRenderTarget = true;
    TexDesc.isUAV = false;
    TexDesc.initialState = nvrhi::ResourceStates::RenderTarget;
    TexDesc.keepInitialState = true;

    TexDesc.format = nvrhi::Format::RGBA16_FLOAT;
    TexDesc.debugName = "GBufferDiffuse";
    GBufferDiffuseTexture = Device->createTexture(TexDesc);
    TexDesc.debugName = "GBufferSpecular";
    GBufferSpecularTexture = Device->createTexture(TexDesc);
    TexDesc.debugName = "GBufferNormals";
    GBufferNormalsTexture = Device->createTexture(TexDesc);
    TexDesc.debugName = "GBufferEmissive";
    GBufferEmissiveTexture = Device->createTexture(TexDesc);

    TexDesc.format = nvrhi::Format::D32;
    TexDesc.isTypeless = true;
    TexDesc.initialState = nvrhi::ResourceStates::DepthWrite;
    TexDesc.debugName = "GBufferDepth";
    GBufferDepthTexture = Device->createTexture(TexDesc);

    // Recreate framebuffer
    nvrhi::FramebufferDesc FBDesc;
    FBDesc.addColorAttachment(GBufferDiffuseTexture);
    FBDesc.addColorAttachment(GBufferSpecularTexture);
    FBDesc.addColorAttachment(GBufferNormalsTexture);
    FBDesc.addColorAttachment(GBufferEmissiveTexture);
    FBDesc.setDepthAttachment(GBufferDepthTexture);
    GBufferFramebuffer = Device->createFramebuffer(FBDesc);

    // Recreate pipeline with new framebuffer info
    nvrhi::GraphicsPipelineDesc PipelineDesc;
    PipelineDesc.setVertexShader(GBufferVS)
        .setPixelShader(GBufferPS)
        .setInputLayout(GBufferInputLayout)
        .addBindingLayout(GBufferBindingLayout)
        .setPrimType(nvrhi::PrimitiveType::TriangleList);
    PipelineDesc.renderState.setRasterState(nvrhi::RasterState().setCullNone());
    PipelineDesc.renderState.depthStencilState
        .setDepthTestEnable(true)
        .setDepthWriteEnable(true)
        .setDepthFunc(nvrhi::ComparisonFunc::Less);

    GBufferPipeline = Device->createGraphicsPipeline(PipelineDesc, GBufferFramebuffer->getFramebufferInfo());

    // Recreate instanced pipeline with new framebuffer info
    nvrhi::GraphicsPipelineDesc InstancedPipelineDesc;
    InstancedPipelineDesc.setVertexShader(GBufferInstancedVS)
        .setPixelShader(GBufferPS)
        .setInputLayout(GBufferInputLayout)
        .addBindingLayout(GBufferInstancedBindingLayout)
        .setPrimType(nvrhi::PrimitiveType::TriangleList);
    InstancedPipelineDesc.renderState.setRasterState(nvrhi::RasterState().setCullNone());
    InstancedPipelineDesc.renderState.depthStencilState
        .setDepthTestEnable(true)
        .setDepthWriteEnable(true)
        .setDepthFunc(nvrhi::ComparisonFunc::Less);

    GBufferInstancedPipeline = Device->createGraphicsPipeline(InstancedPipelineDesc, GBufferFramebuffer->getFramebufferInfo());
}

void FGBufferFillPass::Shutdown()
{
    GBufferVS = nullptr;
    GBufferPS = nullptr;
    GBufferInputLayout = nullptr;
    GBufferBindingLayout = nullptr;
    GBufferPipeline = nullptr;
    GBufferInstancedVS = nullptr;
    GBufferInstancedBindingLayout = nullptr;
    GBufferInstancedPipeline = nullptr;
    ViewConstantsBuffer = nullptr;
    MaterialConstantBuffer = nullptr;
    GBufferLinearSampler = nullptr;
    GBufferDiffuseTexture = nullptr;
    GBufferSpecularTexture = nullptr;
    GBufferNormalsTexture = nullptr;
    GBufferEmissiveTexture = nullptr;
    GBufferDepthTexture = nullptr;
    GBufferFramebuffer = nullptr;
    Device = nullptr;
    bIsInitialized = false;
}
