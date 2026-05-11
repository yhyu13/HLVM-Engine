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

#pragma once

#include "Renderer/Deferred/FGBufferTextures.h"
#include "Renderer/Deferred/FViewConstants.h"
#include "Renderer/RHI/Object/Buffer.h"
#include <nvrhi/nvrhi.h>
#include <memory>

class FGBufferFillPass
{
public:
    FGBufferFillPass();
    ~FGBufferFillPass();

    FGBufferFillPass(const FGBufferFillPass&) = delete;
    FGBufferFillPass& operator=(const FGBufferFillPass&) = delete;
    FGBufferFillPass(FGBufferFillPass&&) = delete;
    FGBufferFillPass& operator=(FGBufferFillPass&&) = delete;

    /**
     * Initialize the GBuffer fill pass.
     * @param Device NVRHI device
     * @param GBuffer GBuffer textures to render into
     * @param ViewConstants View/projection constants
     * @return true if initialization succeeded
     */
    [[nodiscard]] bool Initialize(
        nvrhi::IDevice* Device,
        FGBufferTextures* GBuffer,
        const FViewConstants* ViewConstants);

    /**
     * Render the cube to the GBuffer.
     * @param CommandList Command list to record commands
     */
    void Render(nvrhi::ICommandList* CommandList);

    /**
     * Cleanup resources.
     */
    void Cleanup();

private:
    nvrhi::IDevice* Device = nullptr;
    FGBufferTextures* GBuffer = nullptr;
    const FViewConstants* ViewConstants = nullptr;

    // Shaders
    nvrhi::ShaderHandle VertexShader;
    nvrhi::ShaderHandle PixelShader;
    nvrhi::SamplerHandle AnisotropicSampler;

    // Pipeline
    nvrhi::InputLayoutHandle InputLayout;
    
    // Binding layouts (2 required for GBuffer pipeline)
    // Set 0: Material textures + samplers
    nvrhi::BindingLayoutHandle MaterialBindingLayout;
    // Set 2: ViewConstants (volatile) + sampler
    nvrhi::BindingLayoutHandle ViewBindingLayout;
    nvrhi::GraphicsPipelineHandle Pipeline;

    // Buffers
    TUniquePtr<FStaticVertexBuffer> PositionBuffer;
    TUniquePtr<FStaticVertexBuffer> TexCoordBuffer;
    TUniquePtr<FStaticVertexBuffer> NormalBuffer;
    TUniquePtr<FStaticVertexBuffer> TangentBuffer;
    TUniquePtr<FStaticIndexBuffer> IndexBuffer;

    // Binding sets
    nvrhi::BindingSetHandle MaterialBindingSet;
    nvrhi::BindingSetHandle ViewBindingSet;

    bool bIsInitialized = false;

    bool CreateMaterialBindingLayout();
    bool CreateViewBindingLayout();
    bool CreatePipelines();
    bool CreateGeometryBuffers(nvrhi::ICommandList* CmdList);

    bool UpdateMaterialBindingSet();
    bool UpdateViewBindingSet();
};
