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

#include "Math/MathGLM.h"
#include <nvrhi/nvrhi.h>
#include <cstdint>

/**
 * GBuffer texture types for deferred rendering.
 * Mirrors Donut's GBufferRenderTargets enum order.
 */
enum class EGBufferTexture : uint8_t
{
    Depth = 0,
    GBufferDiffuse,    // RGB: diffuse albedo, W: opacity
    GBufferSpecular,   // RGB: specular F0, W: occlusion
    GBufferNormals,    // RGB: shading normal, W: roughness
    GBufferEmissive,   // RGB: emissive color, W: unused
    Count
};

/**
 * FGBufferTextures - Manages GBuffer render targets for deferred shading.
 * 
 * Creates and manages the 5 GBuffer textures plus optional motion vectors.
 * The framebuffer binds 4 color targets (Diffuse, Specular, Normals, Emissive)
 * plus a depth buffer.
 */
class FGBufferTextures
{
public:
    FGBufferTextures() = default;
    virtual ~FGBufferTextures();

    FGBufferTextures(const FGBufferTextures&) = delete;
    FGBufferTextures& operator=(const FGBufferTextures&) = delete;
    FGBufferTextures(FGBufferTextures&&) = delete;
    FGBufferTextures& operator=(FGBufferTextures&&) = delete;

    /**
     * Create GBuffer textures with given dimensions.
     * @param Device NVRHI device
     * @param Width Frame width in pixels
     * @param Height Frame height in pixels
     * @param SampleCount MSAA sample count (1 = no MSAA)
     * @param bEnableMotionVectors Create motion vector texture (default false)
     * @param bUseReverseProjection Use reverse-Z depth (default false)
     * @return true if creation succeeded
     */
    [[nodiscard]] bool Create(
        nvrhi::IDevice* Device,
        uint32_t Width,
        uint32_t Height,
        uint32_t SampleCount = 1,
        bool bEnableMotionVectors = false,
        bool bUseReverseProjection = false);

    /**
     * Clear all GBuffer textures to default values.
     * @param CommandList Command list to record clear commands
     */
    void Clear(nvrhi::ICommandList* CommandList);

    /**
     * Get a specific GBuffer texture.
     * @param Type The texture type to retrieve
     * @return Texture handle or nullptr if not created
     */
    [[nodiscard]] nvrhi::TextureHandle GetTexture(EGBufferTexture Type) const;

    /**
     * Get the depth texture handle.
     * @return Depth texture handle
     */
    [[nodiscard]] nvrhi::TextureHandle GetDepthTexture() const { return Textures[static_cast<size_t>(EGBufferTexture::Depth)]; }

    /**
     * Get the GBuffer framebuffer for rendering.
     * @return Framebuffer handle
     */
    [[nodiscard]] nvrhi::FramebufferHandle GetFramebuffer() const { return GBufferFramebuffer; }

    /**
     * Get the framebuffer info for pipeline creation.
     * @return Framebuffer info
     */
    [[nodiscard]] const nvrhi::FramebufferInfo& GetFramebufferInfo() const { return FramebufferInfo; }

    /**
     * Get the shaded color output texture (created after GBuffer).
     * @return Shaded color texture
     */
    [[nodiscard]] nvrhi::TextureHandle GetShadedColorTexture() const { return ShadedColorTexture; }

    /**
     * Create the shaded color output texture for deferred lighting output.
     * Must be called after Create().
     * @param Device NVRHI device
     * @return true if creation succeeded
     */
    [[nodiscard]] bool CreateShadedColorTexture(nvrhi::IDevice* Device);

    /**
     * Get GBuffer dimensions.
     * @return uint32_t2 with (width, height)
     */
    [[nodiscard]] uint32_t GetWidth() const { return Width; }
    [[nodiscard]] uint32_t GetHeight() const { return Height; }

private:
    nvrhi::DeviceHandle Device;
    nvrhi::TextureHandle Textures[static_cast<size_t>(EGBufferTexture::Count)];
    nvrhi::FramebufferHandle GBufferFramebuffer;
    nvrhi::FramebufferInfo FramebufferInfo;
    nvrhi::TextureHandle ShadedColorTexture;
    nvrhi::TextureHandle MotionVectorsTexture;
    
    uint32_t Width = 0;
    uint32_t Height = 0;
    uint32_t SampleCount = 1;
    bool bEnableMotionVectors = false;
    bool bUseReverseProjection = false;
    bool bIsCreated = false;

    bool CreateFramebuffer();
};

