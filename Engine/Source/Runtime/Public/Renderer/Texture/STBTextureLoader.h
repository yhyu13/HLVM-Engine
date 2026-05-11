/**
 * STBTextureLoader.h
 * PNG/JPG/BMP texture loading using stb_image.
 */

#pragma once

#include "Renderer/RHI/Object/Texture.h"
#include "Renderer/RHI/RHICommon.h"
#include "Core/String.h"

#include <string>

class FSTBTextureLoader
{
public:
    // Load PNG/JPG/BMP texture from file, returns true on success
    // Path - filesystem path to the image file
    // Device - NVRHI device
    // CommandList - NVRHI command list for upload
    // OutTexture - Output texture (filled on success)
    static bool LoadTexture(
        const std::string& Path,
        nvrhi::IDevice* Device,
        nvrhi::ICommandList* CommandList,
        FTexture& OutTexture);

    // Load PNG/JPG/BMP texture from memory, returns true on success
    // ImageData - raw image bytes (PNG/JPG/BMP encoded)
    // DataSize - size of ImageData in bytes
    // Device - NVRHI device
    // CommandList - NVRHI command list for upload
    // OutTexture - Output texture (filled on success)
    static bool LoadFromMemory(
        const void* ImageData,
        size_t DataSize,
        nvrhi::IDevice* Device,
        nvrhi::ICommandList* CommandList,
        FTexture& OutTexture);
};

