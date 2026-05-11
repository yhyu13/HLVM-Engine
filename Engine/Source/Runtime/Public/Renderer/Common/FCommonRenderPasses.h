// HLVM-Engine: Common render passes (BlitTexture)

#pragma once

#include <cstdint>

#include <nvrhi/nvrhi.h>

#include "FBindingCache.h"
#include "../IRenderPass.h"

/**
 * FCommonRenderPasses - Common rendering utilities
 *
 * Provides shared rendering functionality like texture blitting across the engine.
 * All methods are static - this is a utility struct, not a singleton.
 */
struct FCommonRenderPasses
{
    /**
     * BlitParameters - Configuration for blit operations
     */
    struct BlitParameters
    {
        /** Sampler type for the blit operation */
        enum class EBlitSampler
        {
            Nearest,  // point sampling
            Linear    // linear sampling
        };

        /** Blit mode for visualization */
        enum class EBlitMode
        {
            Normal,  // Normal RGBA display
            Depth    // Show alpha channel as grayscale
        };

        EBlitSampler Sampler = EBlitSampler::Nearest;
        EBlitMode Mode = EBlitMode::Normal;
    };

    /**
     * Create a 1x1 black texture
     */
    static nvrhi::TextureHandle CreateBlackTexture(nvrhi::IDevice* Device);

    /**
     * Create a 1x1 gray texture
     */
    static nvrhi::TextureHandle CreateGrayTexture(nvrhi::IDevice* Device);

    /**
     * Create a 1x1 white texture
     */
    static nvrhi::TextureHandle CreateWhiteTexture(nvrhi::IDevice* Device);

    /**
     * Create a 2x2 black texture (for mipmap generation)
     */
    static nvrhi::TextureHandle CreateBlackTexture2DArray(nvrhi::IDevice* Device);

    /**
     * Create a 2x2 white texture (for mipmap generation)
     */
    static nvrhi::TextureHandle CreateWhiteTexture2DArray(nvrhi::IDevice* Device);

    /**
     * BlitTexture - Copy a texture to the framebuffer
     *
     * @param CommandList The command list to record the blit into
     * @param Framebuffer The destination framebuffer
     * @param SrcTexture Source texture to blit from (must have been added to BindingCache)
     * @param BindingCache Binding cache for creating/finding binding sets
     * @param Width Framebuffer width
     * @param Height Framebuffer height
     * @param Params Blit parameters (sampler type, etc)
     */
    static void BlitTexture(
        nvrhi::ICommandList* CommandList,
        nvrhi::IFramebuffer* Framebuffer,
        nvrhi::TextureHandle SrcTexture,
        FBindingCache* BindingCache,
        uint32_t Width,
        uint32_t Height,
        const BlitParameters& Params);

    /**
     * Shutdown - Release all static Blit resources
     *
     * This should be called before the NVRHI device is destroyed.
     * If BlitTexture is called again after shutdown, it will reinitialize.
     */
    static void Shutdown();

    /**
     * SetShaderDataDir - Override the default shader data directory
     *
     * Use this to specify a custom directory for Blit shaders.
     * Set to empty string to use the default directory.
     *
     * @param Directory The shader data directory to use
     */
    static void SetShaderDataDir(const FString& Directory);
};
