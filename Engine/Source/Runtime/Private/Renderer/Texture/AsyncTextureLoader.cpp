/**
 * AsyncTextureLoader.cpp
 * Async texture decode + GPU upload helpers.
 */

#include "Renderer/Texture/AsyncTextureLoader.h"
#include "Core/Log.h"
#include "Renderer/RHI/Object/Texture.h"

// stb_image is already implemented in STBTextureLoader.cpp; just get declarations here
#include "stb_image_wrapper.h"

// KTX2 decode
#include <ktx.h>
#include <mutex>

DECLARE_LOG_CATEGORY(LogTexture)

// Vulkan format values
#ifndef VK_FORMAT_R8G8B8A8_UNORM
#define VK_FORMAT_R8G8B8A8_UNORM 37
#endif
#ifndef VK_FORMAT_R8G8B8A8_SRGB
#define VK_FORMAT_R8G8B8A8_SRGB 43
#endif

#ifndef KTX_TTF_RGBA32
#define KTX_TTF_RGBA32 13
#endif

static nvrhi::Format VkFormatToNVRHIFormat(uint32_t vkFormat)
{
    switch (vkFormat)
    {
        case VK_FORMAT_R8G8B8A8_UNORM:
            return nvrhi::Format::RGBA8_UNORM;
        case VK_FORMAT_R8G8B8A8_SRGB:
            return nvrhi::Format::SRGBA8_UNORM;
        default:
            return nvrhi::Format::UNKNOWN;
    }
}

FDecodedImage FAsyncTextureLoader::DecodeSTBTexture(const FPath& TexturePath)
{
    FDecodedImage Result;

    int Width = 0, Height = 0, Channels = 0;
    stbi_uc* Data = stbi_load(
        TexturePath.string().c_str(),
        &Width, &Height, &Channels,
        STBI_rgb_alpha);

    if (Data == nullptr)
    {
        const char* Reason = stbi_failure_reason();
        HLVM_LOG(LogTexture, err,
            TXT("AsyncTextureLoader: Failed to decode '{}': {}"),
            *FString(TexturePath.string().c_str()),
            *FString(Reason ? Reason : "unknown"));
        return Result;
    }

    Result.Width = static_cast<uint32_t>(Width);
    Result.Height = static_cast<uint32_t>(Height);
    Result.RowPitch = Result.Width * 4u;
    Result.MipLevels = 1;
    Result.Format = nvrhi::Format::RGBA8_UNORM;
    Result.bIsValid = true;

    const size_t DataSize = static_cast<size_t>(Width) * static_cast<size_t>(Height) * 4u;
    Result.PixelData.resize(DataSize);
    memcpy(Result.PixelData.data(), Data, DataSize);
    stbi_image_free(Data);

    return Result;
}

FDecodedImage FAsyncTextureLoader::DecodeKTXTexture(const FPath& TexturePath)
{
    FDecodedImage Result;

    ktxTexture* KTXTexture = nullptr;
    ktxResult KtxResult = ktxTexture_CreateFromNamedFile(
        TexturePath.string().c_str(),
        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
        &KTXTexture);

    if (KtxResult != KTX_SUCCESS || KTXTexture == nullptr)
    {
        HLVM_LOG(LogTexture, err,
            TXT("AsyncTextureLoader: Failed to load KTX '{}': {}"),
            *FString(TexturePath.string().c_str()),
            static_cast<int>(KtxResult));
        return Result;
    }

    ktxTexture2* KTXTexture2 = reinterpret_cast<ktxTexture2*>(KTXTexture);
    uint32_t VkFormat = KTXTexture2->vkFormat;
    ktxSupercmpScheme scheme = KTXTexture2->supercompressionScheme;

    bool needsTranscode = (ktxTexture_NeedsTranscoding(KTXTexture) == KTX_TRUE);
    if (VkFormat == 0)
        needsTranscode = true;

    // Decompress supercompression
    if (scheme == KTX_SS_ZSTD)
    {
        KtxResult = ktxTexture2_DeflateZstd(KTXTexture2, 3);
        if (KtxResult != KTX_SUCCESS)
        {
            HLVM_LOG(LogTexture, err,
                TXT("AsyncTextureLoader: KTX Zstd decompression failed: {}"),
                static_cast<int>(KtxResult));
            ktxTexture_Destroy(KTXTexture);
            return Result;
        }
    }
    else if (scheme == KTX_SS_ZLIB)
    {
        KtxResult = ktxTexture2_DeflateZLIB(KTXTexture2, 6);
        if (KtxResult != KTX_SUCCESS)
        {
            HLVM_LOG(LogTexture, err,
                TXT("AsyncTextureLoader: KTX Zlib decompression failed: {}"),
                static_cast<int>(KtxResult));
            ktxTexture_Destroy(KTXTexture);
            return Result;
        }
    }

    needsTranscode = (ktxTexture_NeedsTranscoding(KTXTexture) == KTX_TRUE);
    if (KTXTexture2->vkFormat == 0)
        needsTranscode = true;

    if (needsTranscode)
    {
        // Basis Universal transcoder uses global state and is NOT thread-safe.
        // Serialize transcoding across all worker threads.
        static std::mutex TranscodeMutex;
        std::lock_guard<std::mutex> Lock(TranscodeMutex);

        KtxResult = ktxTexture2_TranscodeBasis(KTXTexture2,
            static_cast<ktx_transcode_fmt_e>(KTX_TTF_RGBA32),
            ktx_transcode_flags{0});
        if (KtxResult != KTX_SUCCESS)
        {
            HLVM_LOG(LogTexture, err,
                TXT("AsyncTextureLoader: KTX transcode failed: {}"),
                static_cast<int>(KtxResult));
            ktxTexture_Destroy(KTXTexture);
            return Result;
        }
        VkFormat = VK_FORMAT_R8G8B8A8_UNORM;
    }

    nvrhi::Format NVRHIFormat = VkFormatToNVRHIFormat(VkFormat);
    if (NVRHIFormat == nvrhi::Format::UNKNOWN)
        NVRHIFormat = nvrhi::Format::RGBA8_UNORM;

    uint32_t Width = KTXTexture->baseWidth;
    uint32_t Height = KTXTexture->baseHeight;
    uint32_t MipLevels = KTXTexture->numLevels;

    Result.Width = Width;
    Result.Height = Height;
    Result.MipLevels = MipLevels;
    Result.Format = NVRHIFormat;
    Result.bIsValid = true;

    // Copy all mip data into our buffer with per-mip info
    Result.Mips.reserve(MipLevels);
    size_t TotalSize = 0;
    for (uint32_t mip = 0; mip < MipLevels; ++mip)
    {
        ktx_size_t offset;
        ktxTexture_GetImageOffset(KTXTexture, mip, 0, 0, &offset);
        uint32_t rowPitch = static_cast<uint32_t>(ktxTexture_GetRowPitch(KTXTexture, mip));
        uint32_t mipHeight = std::max(1u, Height >> mip);
        size_t mipSize = static_cast<size_t>(rowPitch) * mipHeight;

        Result.Mips.push_back({rowPitch, TotalSize});
        TotalSize += mipSize;
    }

    Result.PixelData.resize(TotalSize);
    for (uint32_t mip = 0; mip < MipLevels; ++mip)
    {
        ktx_size_t srcOffset;
        ktxTexture_GetImageOffset(KTXTexture, mip, 0, 0, &srcOffset);
        uint32_t rowPitch = Result.Mips[mip].RowPitch;
        uint32_t mipHeight = std::max(1u, Height >> mip);
        size_t mipSize = static_cast<size_t>(rowPitch) * mipHeight;
        memcpy(Result.PixelData.data() + Result.Mips[mip].Offset,
               reinterpret_cast<const char*>(KTXTexture->pData) + srcOffset,
               mipSize);
    }

    ktxTexture_Destroy(KTXTexture);

    HLVM_LOG(LogTexture, info,
        TXT("AsyncTextureLoader: Decoded KTX '{}': {}x{}x{} mips"),
        *FString(TexturePath.string().c_str()),
        Width, Height, MipLevels);

    return Result;
}

bool FAsyncTextureLoader::UploadTextureToCommandList(
    nvrhi::IDevice* Device,
    nvrhi::ICommandList* CmdList,
    const FDecodedImage& Decoded,
    FTexture& OutTexture)
{
    HLVM_ENSURE_F(Device != nullptr, TXT("UploadTextureToCommandList: Device is null"));
    HLVM_ENSURE_F(CmdList != nullptr, TXT("UploadTextureToCommandList: CmdList is null"));
    HLVM_ENSURE_F(Decoded.bIsValid, TXT("UploadTextureToCommandList: Decoded image is invalid"));

    if (!OutTexture.Initialize(
            Decoded.Width,
            Decoded.Height,
            Decoded.MipLevels,
            static_cast<ETextureFormat>(Decoded.Format),
            ETextureDimension::Texture2D,
            Device,
            nullptr,
            nullptr))
    {
        HLVM_LOG(LogTexture, err, TXT("UploadTextureToCommandList: Failed to initialize texture"));
        return false;
    }

    nvrhi::ITexture* Texture = OutTexture.GetTextureHandle();
    if (!Texture)
    {
        HLVM_LOG(LogTexture, err, TXT("UploadTextureToCommandList: Texture handle is null"));
        return false;
    }

    for (uint32_t mip = 0; mip < Decoded.MipLevels; ++mip)
    {
        uint32_t rowPitch;
        const void* data;
        if (!Decoded.Mips.empty())
        {
            rowPitch = Decoded.Mips[mip].RowPitch;
            data = Decoded.PixelData.data() + Decoded.Mips[mip].Offset;
        }
        else
        {
            rowPitch = Decoded.RowPitch;
            data = Decoded.PixelData.data();
        }
        CmdList->writeTexture(Texture, 0, mip, data, rowPitch);
    }

    return true;
}
