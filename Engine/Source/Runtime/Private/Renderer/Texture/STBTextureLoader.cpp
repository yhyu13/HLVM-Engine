/**
 * STBTextureLoader.cpp
 * PNG/JPG/BMP texture loading implementation using stb_image.
 * Uses local ThirdParty/stb/stb_image.h (header-only, no LTO issues).
 */

#include "Renderer/Texture/STBTextureLoader.h"

#include "Core/Log.h"
#include "Core/Assert.h"
#include <cstdio>
#include <cstring>

// Include stb_image directly with implementation
// Suppress warnings for old-style casts in stb
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#pragma clang diagnostic ignored "-Wunused-macros"
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

DECLARE_LOG_CATEGORY(LogTexture)

bool FSTBTextureLoader::LoadTexture(
    const std::string& Path,
    nvrhi::IDevice* Device,
    nvrhi::ICommandList* CommandList,
    FTexture& OutTexture)
{
    HLVM_ENSURE_F(!Path.empty(), TXT("STBTextureLoader: Path is empty"));
    HLVM_ENSURE_F(Device != nullptr, TXT("STBTextureLoader: Device is null"));
    HLVM_ENSURE_F(CommandList != nullptr, TXT("STBTextureLoader: CommandList is null"));

    int width, height, channels;
    stbi_uc* imageData = stbi_load(Path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (imageData == nullptr)
    {
        const char* reason = stbi_failure_reason();
        HLVM_LOG(LogTexture, err, TXT("STBTextureLoader: Failed to load image: {}"), *FString(reason ? reason : "unknown error"));
        CommandList->close();
        return false;
    }

    HLVM_LOG(LogTexture, info, TXT("STBTextureLoader: Loaded image: %dx%d, %d channels"),
        width, height, channels);

    // Determine NVRHI format - always use RGBA8 since we force STBI_rgb_alpha
    nvrhi::Format format = nvrhi::Format::RGBA8_UNORM;

    // Calculate row pitch (width * 4 bytes per pixel)
    uint32_t rowPitch = static_cast<uint32_t>(width) * 4u;

    // Create NVRHI texture
    if (!OutTexture.Initialize(
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
            1, // mip levels
            format,
            nvrhi::TextureDimension::Texture2D,
            Device,
            nullptr,
            CommandList))
    {
        HLVM_LOG(LogTexture, err, TXT("STBTextureLoader: Failed to initialize texture"));
        stbi_image_free(imageData);
        return false;
    }

    // Validate texture was created
    if (!OutTexture.GetTextureHandle())
    {
        HLVM_LOG(LogTexture, err, TXT("STBTextureLoader: Texture handle invalid after init"));
        stbi_image_free(imageData);
        return false;
    }

    // Upload texture data
    HLVM_LOG(LogTexture, debug, TXT("STBTextureLoader: Uploading texture data, rowPitch=%u"), rowPitch);

    CommandList->writeTexture(
        OutTexture.GetTextureHandle(),
        0, // array slice
        0, // mip level
        reinterpret_cast<const char*>(imageData),
        rowPitch);

    // CRITICAL FIX: Execute command list before freeing source data
    // The GPU has not yet read the image data until the command list is executed.
    // Freeing imageData before executeCommandList would cause UAF.
    CommandList->close();
    Device->executeCommandList(CommandList);

    // Safe to free now - GPU has completed reading the data
    stbi_image_free(imageData);

    HLVM_LOG(LogTexture, info, TXT("STBTextureLoader: Texture upload complete"));
    return true;
}

bool FSTBTextureLoader::LoadFromMemory(
    const void* ImageData,
    size_t DataSize,
    nvrhi::IDevice* Device,
    nvrhi::ICommandList* CommandList,
    FTexture& OutTexture)
{
    HLVM_ENSURE_F(ImageData != nullptr, TXT("STBTextureLoader::LoadFromMemory: ImageData is null"));
    HLVM_ENSURE_F(DataSize > 0, TXT("STBTextureLoader::LoadFromMemory: DataSize is zero"));
    HLVM_ENSURE_F(Device != nullptr, TXT("STBTextureLoader::LoadFromMemory: Device is null"));
    HLVM_ENSURE_F(CommandList != nullptr, TXT("STBTextureLoader::LoadFromMemory: CommandList is null"));

    int width, height, channels;
    stbi_uc* imageData = stbi_load_from_memory(
        static_cast<const stbi_uc*>(ImageData),
        static_cast<int>(DataSize),
        &width, &height, &channels,
        STBI_rgb_alpha);

    if (imageData == nullptr)
    {
        HLVM_LOG(LogTexture, err, TXT("STBTextureLoader::LoadFromMemory: Failed to decode image: {}"), *FString(stbi_failure_reason()));
        CommandList->close();
        return false;
    }

    HLVM_LOG(LogTexture, info, TXT("STBTextureLoader::LoadFromMemory: Decoded image: %dx%d, %d channels"),
        width, height, channels);

    // Determine NVRHI format - always use RGBA8 since we force STBI_rgb_alpha
    nvrhi::Format format = nvrhi::Format::RGBA8_UNORM;

    // Calculate row pitch (width * 4 bytes per pixel)
    uint32_t rowPitch = static_cast<uint32_t>(width) * 4u;

    // Create NVRHI texture
    if (!OutTexture.Initialize(
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
            1, // mip levels
            format,
            nvrhi::TextureDimension::Texture2D,
            Device,
            nullptr,
            CommandList))
    {
        HLVM_LOG(LogTexture, err, TXT("STBTextureLoader::LoadFromMemory: Failed to initialize texture"));
        stbi_image_free(imageData);
        return false;
    }

    // Validate texture was created
    if (!OutTexture.GetTextureHandle())
    {
        HLVM_LOG(LogTexture, err, TXT("STBTextureLoader::LoadFromMemory: Texture handle invalid after init"));
        stbi_image_free(imageData);
        return false;
    }

    // Upload texture data
    HLVM_LOG(LogTexture, debug, TXT("STBTextureLoader::LoadFromMemory: Uploading texture data, rowPitch=%u"), rowPitch);

    CommandList->writeTexture(
        OutTexture.GetTextureHandle(),
        0, // array slice
        0, // mip level
        reinterpret_cast<const char*>(imageData),
        rowPitch);

    // Execute command list before freeing source data
    CommandList->close();
    Device->executeCommandList(CommandList);

    // Safe to free now - GPU has completed reading the data
    stbi_image_free(imageData);

    HLVM_LOG(LogTexture, info, TXT("STBTextureLoader::LoadFromMemory: Texture upload complete"));
    return true;
}
