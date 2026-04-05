/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * ImGui NVRHI Renderer Header (MVP-based)
 *
 * Provides NVRHI-based rendering for Dear ImGui using MVP matrix approach.
 * Adapted from NVIDIA Donut framework.
 */

#pragma once

#include <memory>
#include <map>
#include <unordered_map>
#include <cstdint>

#include <nvrhi/nvrhi.h>

#include <imgui.h>

class FShaderFactory;

struct VERTEX_CONSTANT_BUFFER
{
    float mvp[4][4];
};

class FImgui_NVRHI
{
public:
    nvrhi::DeviceHandle Device;
    nvrhi::CommandListHandle CommandList;
    std::shared_ptr<FShaderFactory> ShaderFactory;

    nvrhi::ShaderHandle VertexShader;
    nvrhi::ShaderHandle PixelShader;
    nvrhi::InputLayoutHandle ShaderAttribLayout;

    nvrhi::TextureHandle FontTexture;
    nvrhi::SamplerHandle FontSampler;

    nvrhi::BufferHandle VertexBuffer;
    nvrhi::BufferHandle IndexBuffer;

    nvrhi::BindingLayoutHandle BindingLayout;
    nvrhi::GraphicsPipelineDesc PipelineDesc;
    nvrhi::GraphicsPipelineHandle Pipeline;

    std::map<std::pair<nvrhi::ITexture*, nvrhi::IGraphicsPipeline*>, nvrhi::BindingSetHandle> BindingsCache;

    std::vector<ImDrawVert> VertexBufferData;
    std::vector<ImDrawIdx> IndexBufferData;

    bool Initialize(nvrhi::IDevice* device, std::shared_ptr<FShaderFactory> shaderFactory);
    bool UpdateFontTexture();
    void Render(nvrhi::IFramebuffer* framebuffer);
    void BackBufferResizing();
    void Shutdown();

private:
    bool ReallocateBuffer(nvrhi::BufferHandle& buffer, size_t requiredSize, size_t reallocateSize, bool isIndexBuffer);
    nvrhi::IGraphicsPipeline* GetPipeline(nvrhi::FramebufferInfo const& framebufferInfo);
    nvrhi::IBindingSet* GetBindingSet(nvrhi::ITexture* texture, nvrhi::IGraphicsPipeline* pipeline);
    bool UpdateGeometry(nvrhi::ICommandList* commandList);
};
