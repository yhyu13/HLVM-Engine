/**
 * KTXTextureLoader.cpp
 * KTX2 texture loading implementation.
 */

#include "Renderer/Texture/KTXTextureLoader.h"

#include "Core/Log.h"
#include "Core/Assert.h"

#include <ktx.h>

DECLARE_LOG_CATEGORY(LogTexture)

// Vulkan format values (from vulkan_core.h)
#ifndef VK_FORMAT_R8G8B8A8_UNORM
#define VK_FORMAT_R8G8B8A8_UNORM 37
#endif
#ifndef VK_FORMAT_R8G8B8A8_SRGB
#define VK_FORMAT_R8G8B8A8_SRGB 43
#endif
#ifndef VK_FORMAT_B8G8R8A8_UNORM
#define VK_FORMAT_B8G8R8A8_UNORM 44
#endif

// KTX transcode target format - uncompressed RGBA32
#ifndef KTX_TTF_RGBA32
#define KTX_TTF_RGBA32 13
#endif

/**
 * @brief Convert Vulkan format to NVRHI format
 */
static nvrhi::Format VkFormatToNVRHIFormat(uint32_t vkFormat)
{
    switch (vkFormat)
    {
        case VK_FORMAT_R8G8B8A8_UNORM:
            return nvrhi::Format::RGBA8_UNORM;
        case VK_FORMAT_R8G8B8A8_SRGB:
            return nvrhi::Format::SRGBA8_UNORM;
        case VK_FORMAT_B8G8R8A8_UNORM:
            return nvrhi::Format::BGRA8_UNORM;
        default:
            return nvrhi::Format::UNKNOWN;
    }
}

bool FKTXTextureLoader::LoadKTX2Texture(
    const std::string&         Path,
    nvrhi::IDevice*            Device,
    nvrhi::ICommandList*       CommandList,
    FTexture&                  OutTexture)
{
    HLVM_ENSURE_F(!Path.empty(), TXT("KTXTextureLoader: Path is empty"));
    HLVM_ENSURE_F(Device != nullptr, TXT("KTXTextureLoader: Device is null"));
    HLVM_ENSURE_F(CommandList != nullptr, TXT("KTXTextureLoader: CommandList is null"));

    // Create KTX texture from file
    ktxTexture* KTXTexture = nullptr;
    ktxResult result = ktxTexture_CreateFromNamedFile(Path.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &KTXTexture);
    if (result != KTX_SUCCESS || KTXTexture == nullptr)
    {
        HLVM_LOG(LogTexture, err, TXT("KTXTextureLoader: Failed to create KTX texture: {}"), static_cast<int>(result));
        return false;
    }

    // Get texture dimensions
    uint32_t Width     = KTXTexture->baseWidth;
    uint32_t Height    = KTXTexture->baseHeight;
    uint32_t MipLevels = KTXTexture->numLevels;

    HLVM_LOG(LogTexture, info, TXT("KTXTextureLoader: Loaded KTX texture: {}x{}, {} mip levels"),
        Width, Height, MipLevels);

    // Cast to ktxTexture2 for format access
    ktxTexture2* KTXTexture2 = reinterpret_cast<ktxTexture2*>(KTXTexture);
    uint32_t     VkFormat    = KTXTexture2->vkFormat;
    ktxSupercmpScheme scheme = KTXTexture2->supercompressionScheme;

    HLVM_LOG(LogTexture, info, TXT("KTXTextureLoader: vkFormat = {}, supercompressionScheme = {}"), VkFormat, static_cast<int>(scheme));

    // Check if we need to transcode (compressed format)
    bool needsTranscode = (ktxTexture_NeedsTranscoding(KTXTexture) == KTX_TRUE);

    // If vkFormat is 0 (UNDEFINED), we need to transcode regardless of needsTranscode
    // This handles KTX2 files with engine-specific formats
    if (VkFormat == 0)
    {
        HLVM_LOG(LogTexture, info, TXT("KTXTextureLoader: vkFormat is UNDEFINED, forcing transcode"));
        needsTranscode = true;
    }

    // Step 1: Handle supercompression (Zstd, Zlib, etc.)
    // After deflation, the inner compressed data (ETC2, BC7, ASTC, etc.) is revealed
    if (scheme == KTX_SS_ZSTD)
    {
        HLVM_LOG(LogTexture, info, TXT("KTXTextureLoader: Decompressing Zstd supercompression layer"));
        result = ktxTexture2_DeflateZstd(KTXTexture2, 3); // level 3 (default)
        if (result != KTX_SUCCESS)
        {
            HLVM_LOG(LogTexture, err, TXT("KTXTextureLoader: Zstd decompression failed with error: {}"), static_cast<int>(result));
            ktxTexture_Destroy(KTXTexture);
            return false;
        }
        HLVM_LOG(LogTexture, info, TXT("KTXTextureLoader: Zstd decompressed, vkFormat={}"), KTXTexture2->vkFormat);
    }
    else if (scheme == KTX_SS_ZLIB)
    {
        HLVM_LOG(LogTexture, info, TXT("KTXTextureLoader: Decompressing Zlib supercompression layer"));
        result = ktxTexture2_DeflateZLIB(KTXTexture2, 6); // level 6 (default)
        if (result != KTX_SUCCESS)
        {
            HLVM_LOG(LogTexture, err, TXT("KTXTextureLoader: Zlib decompression failed with error: {}"), static_cast<int>(result));
            ktxTexture_Destroy(KTXTexture);
            return false;
        }
        HLVM_LOG(LogTexture, info, TXT("KTXTextureLoader: Zlib decompressed, vkFormat={}"), KTXTexture2->vkFormat);
    }

    // Step 2: Check if we still need to transcode after deflation
    // This handles ETC2, BC1-7, ASTC, UASTC, etc.
    needsTranscode = (ktxTexture_NeedsTranscoding(KTXTexture) == KTX_TRUE);

    // If vkFormat is 0 (UNDEFINED), we need to transcode regardless
    if (KTXTexture2->vkFormat == 0)
    {
        needsTranscode = true;
    }

    if (needsTranscode)
    {
        HLVM_LOG(LogTexture, info, TXT("KTXTextureLoader: Transcoding to RGBA32 (vkFormat={})"), KTXTexture2->vkFormat);

        // Transcode to uncompressed RGBA32
        // This works for: ETC1, ETC2, EAC, BC1-5, BC7, ASTC, UASTC, etc.
        result = ktxTexture2_TranscodeBasis(KTXTexture2,
            static_cast<ktx_transcode_fmt_e>(KTX_TTF_RGBA32),
            ktx_transcode_flags{0});

        if (result != KTX_SUCCESS)
        {
            HLVM_LOG(LogTexture, err, TXT("KTXTextureLoader: Transcode failed with error: {}"), static_cast<int>(result));
            HLVM_LOG(LogTexture, err, TXT("KTXTextureLoader: Texture format {} may not be supported for CPU transcoding"),
                KTXTexture2->vkFormat);
            ktxTexture_Destroy(KTXTexture);
            return false;
        }

        // After transcode, vkFormat becomes UNDEFINED, we use RGBA32
        VkFormat = VK_FORMAT_R8G8B8A8_UNORM;
        HLVM_LOG(LogTexture, info, TXT("KTXTextureLoader: Transcode complete, using RGBA32"));
    }

    // Convert Vulkan format to NVRHI format
    nvrhi::Format NVRHIFormat = VkFormatToNVRHIFormat(VkFormat);
    if (NVRHIFormat == nvrhi::Format::UNKNOWN)
    {
        HLVM_LOG(LogTexture, err, TXT("KTXTextureLoader: Unsupported Vulkan format: {}"), VkFormat);
        // Fallback to RGBA8_UNORM for common formats
        NVRHIFormat = nvrhi::Format::RGBA8_UNORM;
        HLVM_LOG(LogTexture, warn, TXT("KTXTextureLoader: Falling back to RGBA8_UNORM"));
    }

    // Create NVRHI texture
    if (!OutTexture.Initialize(Width, Height, MipLevels, NVRHIFormat,
            nvrhi::TextureDimension::Texture2D, Device, nullptr, CommandList))
    {
        HLVM_LOG(LogTexture, err, TXT("KTXTextureLoader: Failed to initialize texture"));
        ktxTexture_Destroy(KTXTexture);
        return false;
    }

    // Validate texture was created
    if (!OutTexture.GetTextureHandle())
    {
        HLVM_LOG(LogTexture, err, TXT("KTXTextureLoader: Texture handle invalid after init"));
        ktxTexture_Destroy(KTXTexture);
        return false;
    }

    // Upload each mip level
    for (uint32_t mip = 0; mip < MipLevels; ++mip)
    {
        ktx_size_t offset;
        ktxTexture_GetImageOffset(KTXTexture, mip, 0, 0, &offset);

        uint32_t rowPitch = ktxTexture_GetRowPitch(KTXTexture, mip);

        HLVM_LOG(LogTexture, debug, TXT("KTXTextureLoader: Uploading mip {} offset={}, rowPitch={}"),
            mip, static_cast<size_t>(offset), rowPitch);

        CommandList->writeTexture(OutTexture.GetTextureHandle(), 0, mip,
            reinterpret_cast<const char*>(KTXTexture->pData) + offset, rowPitch);
    }

    // Execute command list before destroying KTX data
    // This ensures GPU upload completes before we free the source data
    CommandList->close();
    Device->executeCommandList(CommandList);

    HLVM_LOG(LogTexture, info, TXT("KTXTextureLoader: Texture upload complete"));

    ktxTexture_Destroy(KTXTexture);
    return true;
}
