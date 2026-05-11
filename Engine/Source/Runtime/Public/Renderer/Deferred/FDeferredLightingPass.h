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
#include "Renderer/Deferred/FLightingConstants.h"
#include <nvrhi/nvrhi.h>
#include <memory>

class FDeferredLightingPass
{
public:
    FDeferredLightingPass();
    ~FDeferredLightingPass();

    FDeferredLightingPass(const FDeferredLightingPass&) = delete;
    FDeferredLightingPass& operator=(const FDeferredLightingPass&) = delete;
    FDeferredLightingPass(FDeferredLightingPass&&) = delete;
    FDeferredLightingPass& operator=(FDeferredLightingPass&&) = delete;

    /**
     * Initialize the deferred lighting pass.
     * @param Device NVRHI device
     * @param GBuffer GBuffer textures to read from
     * @param OutputTexture Output texture to write shaded color
     * @return true if initialization succeeded
     */
    [[nodiscard]] bool Initialize(
        nvrhi::IDevice* Device,
        FGBufferTextures* GBuffer,
        nvrhi::TextureHandle OutputTexture);

    /**
     * Update lighting constants and prepare for rendering.
     * @param LightingConstants Lighting constants to use
     */
    void SetLightingConstants(const FLightingConstants* LightingConstants);

    /**
     * Execute the deferred lighting compute pass.
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
    nvrhi::TextureHandle OutputTexture;
    const FLightingConstants* LightingConstants = nullptr;

    // Compute shader
    nvrhi::ShaderHandle ComputeShader;

    // Binding layout and set
    nvrhi::BindingLayoutHandle BindingLayout;
    nvrhi::BindingSetHandle BindingSet;

    // Compute pipeline
    nvrhi::ComputePipelineHandle Pipeline;

    // Temporary texture for UAV output (if needed)
    nvrhi::TextureHandle TempOutputTexture;

    bool bIsInitialized = false;

    bool LoadShader(const uint8_t* Data, size_t Bytes);
    bool CreateBindingLayout();
    bool UpdateBindingSet();
};
