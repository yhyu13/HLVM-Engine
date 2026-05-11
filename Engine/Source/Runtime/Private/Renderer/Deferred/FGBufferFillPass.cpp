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
#include "Renderer/Deferred/FCubeGeometry.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
#include "Core/Log.h"
#include <fstream>
#include <vector>

DECLARE_LOG_CATEGORY(LogRenderer)

FGBufferFillPass::FGBufferFillPass() = default;

FGBufferFillPass::~FGBufferFillPass()
{
    Cleanup();
}

void FGBufferFillPass::Cleanup()
{
    VertexShader = nullptr;
    PixelShader = nullptr;
    AnisotropicSampler = nullptr;
    InputLayout = nullptr;
    MaterialBindingLayout = nullptr;
    ViewBindingLayout = nullptr;
    Pipeline = nullptr;
    PositionBuffer.reset();
    TexCoordBuffer.reset();
    NormalBuffer.reset();
    TangentBuffer.reset();
    IndexBuffer.reset();
    MaterialBindingSet = nullptr;
    ViewBindingSet = nullptr;
    bIsInitialized = false;
}

bool FGBufferFillPass::Initialize(
    nvrhi::IDevice* InDevice,
    FGBufferTextures* InGBuffer,
    const FViewConstants* InViewConstants)
{
    if (bIsInitialized)
    {
        Cleanup();
    }

    Device = InDevice;
    GBuffer = InGBuffer;
    ViewConstants = InViewConstants;

    nvrhi::CommandListHandle CmdList = Device->createCommandList();
    CmdList->open();

    const auto DataDir = FString::Format(
        TXT("{}/../../Test/{}_Data"),
        *GExecutablePath,
        *GExecutableName);

    auto VSPath = FPath::Combine(DataDir, TXT("gbuffer_vs.sblob"));
    std::ifstream VSFile(VSPath.string(), std::ios::ate | std::ios::binary);
    if (!VSFile.is_open())
    {
        HLVM_LOG(LogRenderer, err, TXT("Failed to open vertex shader"));
        return false;
    }
    size_t VSBytes = static_cast<size_t>(VSFile.tellg());
    std::vector<uint8_t> VSData(VSBytes);
    VSFile.seekg(0);
    VSFile.read(reinterpret_cast<char*>(VSData.data()), static_cast<std::streamsize>(VSBytes));
    VSFile.close();

    // No permutations for simple shader
    const void* VSBinary = nullptr;
    size_t VSBinarySize = 0;
    if (!ShaderMake::FindPermutationInBlob(VSData.data(), VSData.size(), nullptr, 0, &VSBinary, &VSBinarySize))
    {
        HLVM_LOG(LogRenderer, err, TXT("Failed to extract vertex shader SPIR-V from blob"));
        return false;
    }

    auto PSPath = FPath::Combine(DataDir, TXT("gbuffer_ps.sblob"));
    std::ifstream PSFile(PSPath.string(), std::ios::ate | std::ios::binary);
    if (!PSFile.is_open())
    {
        HLVM_LOG(LogRenderer, err, TXT("Failed to open pixel shader"));
        return false;
    }
    size_t PSBytes = static_cast<size_t>(PSFile.tellg());
    std::vector<uint8_t> PSData(PSBytes);
    PSFile.seekg(0);
    PSFile.read(reinterpret_cast<char*>(PSData.data()), static_cast<std::streamsize>(PSBytes));
    PSFile.close();

    // No permutations for simple shader
    const void* PSBinary = nullptr;
    size_t PSBinarySize = 0;
    if (!ShaderMake::FindPermutationInBlob(PSData.data(), PSData.size(), nullptr, 0, &PSBinary, &PSBinarySize))
    {
        HLVM_LOG(LogRenderer, err, TXT("Failed to extract pixel shader SPIR-V from blob"));
        return false;
    }
    nvrhi::ShaderDesc VSDsc;
    VSDsc.setShaderType(nvrhi::ShaderType::Vertex);
    VSDsc.entryName = "main";
    VertexShader = Device->createShader(VSDsc, VSBinary, VSBinarySize);

    nvrhi::ShaderDesc PSDsc;
    PSDsc.setShaderType(nvrhi::ShaderType::Pixel);
    PixelShader = Device->createShader(PSDsc, PSBinary, PSBinarySize);

    if (!VertexShader || !PixelShader)
    {
        HLVM_LOG(LogRenderer, err, TXT("Failed to create GBuffer shaders"));
        return false;
    }

    // Create anisotropic sampler
    nvrhi::SamplerDesc samplerDesc;
    samplerDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Wrap);
    samplerDesc.setAllFilters(true);
    AnisotropicSampler = Device->createSampler(samplerDesc);
    if (!AnisotropicSampler)
    {
        HLVM_LOG(LogRenderer, err, TXT("Failed to create anisotropic sampler"));
        return false;
    }

    if (!CreateMaterialBindingLayout())
    {
        HLVM_LOG(LogRenderer, err, TXT("Failed to create material binding layout"));
        return false;
    }

    if (!CreateViewBindingLayout())
    {
        HLVM_LOG(LogRenderer, err, TXT("Failed to create view binding layout"));
        return false;
    }

    if (!CreateGeometryBuffers(CmdList.Get()))
    {
        HLVM_LOG(LogRenderer, err, TXT("Failed to create geometry buffers"));
        return false;
    }

    CmdList->close();
    Device->executeCommandList(CmdList);

    if (!CreatePipelines())
    {
        HLVM_LOG(LogRenderer, err, TXT("Failed to create pipelines"));
        return false;
    }

    if (!UpdateMaterialBindingSet())
    {
        HLVM_LOG(LogRenderer, err, TXT("Failed to create material binding set"));
        return false;
    }

    if (!UpdateViewBindingSet())
    {
        HLVM_LOG(LogRenderer, err, TXT("Failed to create view binding set"));
        return false;
    }

    bIsInitialized = true;
    HLVM_LOG(LogRenderer, info, TXT("FGBufferFillPass initialized successfully"));
    return true;
}

bool FGBufferFillPass::CreateMaterialBindingLayout()
{
    // GBUFFER_SPACE_MATERIAL = 0
    nvrhi::BindingLayoutDesc LayoutDesc;
    LayoutDesc.setVisibility(nvrhi::ShaderType::Pixel);
    LayoutDesc.setRegisterSpaceAndDescriptorSet(0);

    // Use offsets=0 to match GLSL bindings directly
    nvrhi::VulkanBindingOffsets Offsets;
    Offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0);
    LayoutDesc.setBindingOffsets(Offsets);

    // b0: MaterialConstants - bRegShift 256 → SPIR-V binding 256
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(256));
    // s0: MaterialSampler - sRegShift 128 → SPIR-V binding 128
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(128));

    // t0-t7: Material textures at bindings 0-7
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(2));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(3));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(4));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(5));
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(6));

    MaterialBindingLayout = Device->createBindingLayout(LayoutDesc);
    return MaterialBindingLayout != nullptr;
}

bool FGBufferFillPass::CreateViewBindingLayout()
{
    // GBUFFER_SPACE_VIEW = 2
    nvrhi::BindingLayoutDesc LayoutDesc;
    LayoutDesc.setVisibility(nvrhi::ShaderType::Vertex | nvrhi::ShaderType::Pixel);
    LayoutDesc.setRegisterSpaceAndDescriptorSet(2);

    // Use offsets=0 to match GLSL bindings directly
    nvrhi::VulkanBindingOffsets Offsets;
    Offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0);
    LayoutDesc.setBindingOffsets(Offsets);

    // b2: ViewConstants - bRegShift 256 → SPIR-V binding 258
    LayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(258));
    // s0: Anisotropic sampler - sRegShift 128 → SPIR-V binding 128


    ViewBindingLayout = Device->createBindingLayout(LayoutDesc);
    return ViewBindingLayout != nullptr;
}

bool FGBufferFillPass::CreateGeometryBuffers(nvrhi::ICommandList* CmdList)
{
    PositionBuffer = std::make_unique<FStaticVertexBuffer>();
    if (!PositionBuffer->Initialize(CmdList, Device, g_Positions, g_PositionsSize))
    {
        return false;
    }

    TexCoordBuffer = std::make_unique<FStaticVertexBuffer>();
    if (!TexCoordBuffer->Initialize(CmdList, Device, g_TexCoords, g_TexCoordsSize))
    {
        return false;
    }

    NormalBuffer = std::make_unique<FStaticVertexBuffer>();
    if (!NormalBuffer->Initialize(CmdList, Device, g_Normals, g_NormalsSize))
    {
        return false;
    }

    TangentBuffer = std::make_unique<FStaticVertexBuffer>();
    if (!TangentBuffer->Initialize(CmdList, Device, g_Tangents, g_TangentsSize))
    {
        return false;
    }

    IndexBuffer = std::make_unique<FStaticIndexBuffer>();
    if (!IndexBuffer->Initialize(CmdList, Device, g_Indices, g_IndicesSize, nvrhi::Format::R32_UINT))
    {
        return false;
    }

    return true;
}

bool FGBufferFillPass::CreatePipelines()
{
    if (!GBuffer)
    {
        return false;
    }

    nvrhi::IFramebuffer* FB = GBuffer->GetFramebuffer();
    if (!FB)
    {
        HLVM_LOG(LogRenderer, err, TXT("GBuffer framebuffer is null"));
        return false;
    }

    nvrhi::VertexAttributeDesc Attrs[8];
    Attrs[0].setBufferIndex(0).setName("POSITION").setFormat(nvrhi::Format::RGB32_FLOAT).setOffset(0).setElementStride(12);
    Attrs[1].setBufferIndex(1).setName("TEXCOORD").setFormat(nvrhi::Format::RG32_FLOAT).setOffset(0).setElementStride(8);
    Attrs[2].setBufferIndex(2).setName("NORMAL").setFormat(nvrhi::Format::RGBA8_SNORM).setOffset(0).setElementStride(4);
    Attrs[3].setBufferIndex(3).setName("TANGENT").setFormat(nvrhi::Format::RGBA8_SNORM).setOffset(0).setElementStride(4);
    Attrs[4].setBufferIndex(4).setName("UV1").setFormat(nvrhi::Format::RG32_FLOAT).setOffset(0).setElementStride(8);
    Attrs[5].setBufferIndex(5).setName("COLOR").setFormat(nvrhi::Format::RGBA32_FLOAT).setOffset(0).setElementStride(16);
    Attrs[6].setBufferIndex(6).setName("BLENDWEIGHTS").setFormat(nvrhi::Format::R32_FLOAT).setOffset(0).setElementStride(4);
    Attrs[7].setBufferIndex(7).setName("BLENDINDICES").setFormat(nvrhi::Format::R32_SINT).setOffset(0).setElementStride(4);

    InputLayout = Device->createInputLayout(Attrs, 8, VertexShader);
    if (!InputLayout)
    {
        HLVM_LOG(LogRenderer, err, TXT("Failed to create input layout"));
        return false;
    }

    nvrhi::GraphicsPipelineDesc PipelineDesc;
    PipelineDesc.setInputLayout(InputLayout)
                .setVertexShader(VertexShader)
                .setPixelShader(PixelShader)
                .addBindingLayout(MaterialBindingLayout)
                .addBindingLayout(ViewBindingLayout);

    PipelineDesc.renderState.depthStencilState.setDepthTestEnable(true)
                          .setDepthWriteEnable(true)
                          .setDepthFunc(nvrhi::ComparisonFunc::Less);

    Pipeline = Device->createGraphicsPipeline(PipelineDesc, FB->getFramebufferInfo());
    if (!Pipeline)
    {
        HLVM_LOG(LogRenderer, err, TXT("Failed to create graphics pipeline"));
        return false;
    }

    return true;
}

bool FGBufferFillPass::UpdateMaterialBindingSet()
{
    nvrhi::BindingSetDesc SetDesc;

    // b0: MaterialConstants buffer at slot 0 -> SPIR-V binding 256
    nvrhi::BufferDesc MaterialBufferDesc;
    MaterialBufferDesc.byteSize = 256;
    MaterialBufferDesc.isConstantBuffer = true;
    MaterialBufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
    MaterialBufferDesc.keepInitialState = true;
    nvrhi::BufferHandle MaterialBuffer = Device->createBuffer(MaterialBufferDesc);
    if (!MaterialBuffer)
    {
        HLVM_LOG(LogRenderer, err, TXT("Failed to create MaterialConstants buffer"));
        return false;
    }

    // Create placeholder 1x1 textures for all material texture slots
    nvrhi::TextureDesc TexDesc;
    TexDesc.format = nvrhi::Format::RGBA16_FLOAT;
    TexDesc.width = 1;
    TexDesc.height = 1;
    TexDesc.depth = 1;
    TexDesc.arraySize = 1;
    TexDesc.mipLevels = 1;
    TexDesc.initialState = nvrhi::ResourceStates::ShaderResource;
    TexDesc.keepInitialState = true;
    TexDesc.isRenderTarget = false;
    TexDesc.isUAV = false;
    TexDesc.isShaderResource = true;
    nvrhi::TextureHandle PlaceholderTex = Device->createTexture(TexDesc);
    if (!PlaceholderTex)
    {
        HLVM_LOG(LogRenderer, err, TXT("Failed to create placeholder texture"));
        return false;
    }
    SetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(256, MaterialBuffer));
    // t0-t6: placeholder textures at slots 0-6
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, PlaceholderTex));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, PlaceholderTex));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(2, PlaceholderTex));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(3, PlaceholderTex));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(4, PlaceholderTex));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(5, PlaceholderTex));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(6, PlaceholderTex));
    SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(7, PlaceholderTex));
    // s0: sampler - bRegShift 128 → SPIR-V binding 128
    SetDesc.addItem(nvrhi::BindingSetItem::Sampler(128, AnisotropicSampler));

    MaterialBindingSet = Device->createBindingSet(SetDesc, MaterialBindingLayout);
    return MaterialBindingSet != nullptr;
}

bool FGBufferFillPass::UpdateViewBindingSet()
{
    nvrhi::BindingSetDesc SetDesc;

    // b2: ViewConstants buffer (volatile) at slot 2 -> SPIR-V binding 258
    nvrhi::BufferDesc ViewBufferDesc;
    ViewBufferDesc.byteSize = sizeof(FViewConstants);
    ViewBufferDesc.isConstantBuffer = true;
    ViewBufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
    ViewBufferDesc.keepInitialState = true;
    nvrhi::BufferHandle ViewConstantsBuffer = Device->createBuffer(ViewBufferDesc);
    if (!ViewConstantsBuffer)
    {
        HLVM_LOG(LogRenderer, err, TXT("Failed to create ViewConstants buffer"));
        return false;
    }

    nvrhi::CommandListHandle UploadCmdList = Device->createCommandList();
    UploadCmdList->open();
    UploadCmdList->writeBuffer(ViewConstantsBuffer, ViewConstants, sizeof(FViewConstants), 0);
    UploadCmdList->close();
    Device->executeCommandList(UploadCmdList);

    // b2: ViewConstants - bRegShift 256 → SPIR-V binding 258
    SetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(258, ViewConstantsBuffer));
    // s0: sampler - sRegShift 128 → SPIR-V binding 128
    SetDesc.addItem(nvrhi::BindingSetItem::Sampler(128, AnisotropicSampler));
    // s0: sampler at slot 0 -> SPIR-V binding 128
    SetDesc.addItem(nvrhi::BindingSetItem::Sampler(0, AnisotropicSampler));

    ViewBindingSet = Device->createBindingSet(SetDesc, ViewBindingLayout);
    return ViewBindingSet != nullptr;
}

void FGBufferFillPass::Render(nvrhi::ICommandList* CommandList)
{
    if (!bIsInitialized || !Pipeline || !MaterialBindingSet || !ViewBindingSet)
    {
        return;
    }

    nvrhi::IFramebuffer* FB = GBuffer->GetFramebuffer();
    if (!FB)
    {
        return;
    }

    nvrhi::GraphicsState State;
    State.pipeline = Pipeline;
    State.framebuffer = FB;
    State.bindings = {MaterialBindingSet, ViewBindingSet};

    nvrhi::VertexBufferBinding PositionBinding;
    PositionBinding.setBuffer(PositionBuffer->GetBufferHandle().Get());
    PositionBinding.setSlot(0);
    PositionBinding.setOffset(0);
    State.addVertexBuffer(PositionBinding);

    nvrhi::VertexBufferBinding TexCoordBinding;
    TexCoordBinding.setBuffer(TexCoordBuffer->GetBufferHandle().Get());
    TexCoordBinding.setSlot(1);
    TexCoordBinding.setOffset(0);
    State.addVertexBuffer(TexCoordBinding);

    nvrhi::VertexBufferBinding NormalBinding;
    NormalBinding.setBuffer(NormalBuffer->GetBufferHandle().Get());
    NormalBinding.setSlot(2);
    NormalBinding.setOffset(0);
    State.addVertexBuffer(NormalBinding);

    nvrhi::VertexBufferBinding TangentBinding;
    TangentBinding.setBuffer(TangentBuffer->GetBufferHandle().Get());
    TangentBinding.setSlot(3);
    TangentBinding.setOffset(0);
    State.addVertexBuffer(TangentBinding);

    nvrhi::IndexBufferBinding IndexBinding;
    IndexBinding.setBuffer(IndexBuffer->GetBufferHandle().Get());
    IndexBinding.setOffset(0);
    IndexBinding.setFormat(nvrhi::Format::R32_UINT);
    State.setIndexBuffer(IndexBinding);

    CommandList->setGraphicsState(State);

    nvrhi::DrawArguments Args;
    Args.vertexCount = g_NumIndices;
    Args.instanceCount = 1;
    Args.startVertexLocation = 0;
    Args.startInstanceLocation = 0;

    CommandList->drawIndexed(Args);
}
