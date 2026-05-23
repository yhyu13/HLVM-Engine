#pragma once

#include "Core/String.h"
#include "Renderer/RHI/Object/Texture.h"
#include "Renderer/Material/PBRMaterial.h"
#include <nvrhi/nvrhi.h>

/**
 * CPU-side decoded image data, ready for GPU upload.
 * Owned by the caller until upload is complete.
 */
struct FDecodedImage
{
    TVector<uint8_t> PixelData;
    uint32_t Width = 0;
    uint32_t Height = 0;
    uint32_t RowPitch = 0; // Used when Mips is empty (single-mip STB path)
    uint32_t MipLevels = 1;
    nvrhi::Format Format = nvrhi::Format::RGBA8_UNORM;
    bool bIsValid = false;

    struct FMipData
    {
        uint32_t RowPitch;
        size_t Offset;
    };
    TVector<FMipData> Mips; // Per-mip row pitch and offset into PixelData
};

/**
 * Async texture loading utility.
 *
 * Usage:
 *   1. FAsyncTextureLoader::DecodeSTBTexture(Path)  // worker thread
 *   2. FAsyncTextureLoader::UploadTextureToCommandList(Device, CmdList, Decoded, OutTexture)  // main thread
 */
class FAsyncTextureLoader
{
public:
    /**
     * Decode an image file from disk to CPU pixel data.
     * Thread-safe. Does NOT touch NVRHI.
     * Supports STB formats: PNG, JPG, BMP, TGA.
     */
    static FDecodedImage DecodeSTBTexture(const FPath& TexturePath);

    /**
     * Decode a KTX2 file from disk to CPU pixel data (decompress + transcode to RGBA32).
     * Thread-safe. Does NOT touch NVRHI.
     */
    static FDecodedImage DecodeKTXTexture(const FPath& TexturePath);

    /**
     * Create an NVRHI texture and append upload commands to an open command list.
     * The command list must not be closed or executed by this function.
     * Supports both single-mip (STB) and multi-mip (KTX) images.
     */
    static bool UploadTextureToCommandList(
        nvrhi::IDevice* Device,
        nvrhi::ICommandList* CmdList,
        const FDecodedImage& Decoded,
        FTexture& OutTexture);

    /**
     * Async decode + batched GPU upload for material textures.
     * Consolidates the common PendingTextures pattern used across tests and scene loading.
     *
     * @param Device         NVRHI device
     * @param Materials      Materials to load textures for
     * @param TypesToLoad    Which texture types to load (e.g., Albedo, Normal)
     * @return Number of textures successfully uploaded
     */
    static uint32_t LoadMaterialTexturesAsync(
        nvrhi::IDevice* Device,
        const TVector<std::shared_ptr<FPBRMaterial>>& Materials,
        const TVector<IMaterial::ETextureType>& TypesToLoad);
};
