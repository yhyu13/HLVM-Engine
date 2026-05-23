/**
 * AsyncTextureLoader.cpp
 * Async texture decode + GPU upload helpers.
 */

#include "Renderer/Texture/AsyncTextureLoader.h"
#include "Renderer/Texture/TextureCache.h"
#include "Core/Log.h"
#include "Renderer/RHI/Object/Texture.h"
#include "Renderer/Material/IMaterial.h"
#include "Renderer/Material/PBRMaterial.h"
#include "Core/Parallel/Async/WorkStealThreadPool.h"

// stb_image is already implemented in STBTextureLoader.cpp; just get declarations here
#include "stb_image_wrapper.h"

// KTX2 decode
#include <ktx.h>
#include <mutex>
#include <future>

DECLARE_LOG_CATEGORY(LogTexture)

// =====================================================================
// Non-blocking async load state
// =====================================================================

struct FPendingTextureLoad
{
    FPath TexturePath;
    std::future<FDecodedImage> Future;
    TVector<FTexture*> TargetTextures;
    nvrhi::IDevice* Device = nullptr;
};

static TVector<FPendingTextureLoad> GPendingLoads;
static std::mutex GPendingLoadsMutex;

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

uint32_t FAsyncTextureLoader::LoadMaterialTexturesAsync(
    nvrhi::IDevice* Device,
    const TVector<std::shared_ptr<FPBRMaterial>>& Materials,
    const TVector<IMaterial::ETextureType>& TypesToLoad)
{
    struct FDecodeTask
    {
        FPath TexturePath;
        std::future<FDecodedImage> Future;
    };

    TMap<FPath, TVector<FTexture*>> PathToTextures;
    TVector<FDecodeTask> DecodeTasks;

    // First pass: check cache and build deduplicated decode list
    for (auto& PBRMat : Materials)
    {
        if (!PBRMat)
            continue;

        for (IMaterial::ETextureType Type : TypesToLoad)
        {
            if (PBRMat->HasTexture(Type) && !PBRMat->HasGPUTexture(Type))
            {
                FPath TexturePath = PBRMat->GetTexturePath(Type);
                FPath AbsolutePath = FPath::Absolute(TexturePath);

                // Check texture cache first
                nvrhi::TextureHandle CachedTexture = FTextureCache::Get().GetTexture(AbsolutePath);
                if (CachedTexture)
                {
                    FTexture& OutTexture = PBRMat->GetGPUTexture(Type);
                    if (OutTexture.InitializeFromHandle(CachedTexture, Device))
                    {
                        continue;
                    }
                }

                FTexture* OutTexture = &PBRMat->GetGPUTexture(Type);

                // Deduplicate: only enqueue one decode task per unique path
                if (!PathToTextures.contains(AbsolutePath))
                {
                    PathToTextures[AbsolutePath] = TVector<FTexture*>{};

                    FString Ext = FPath::GetExtension(TexturePath);
                    if (Ext == ".ktx" || Ext == ".KTX" ||
                        Ext == ".ktx2" || Ext == ".KTX2")
                    {
                        FPath KTX2Path = TexturePath.parent_path() / "ktx2" /
                                         (TexturePath.stem().string() + ".ktx2");
                        FPath DecodePath = std::filesystem::exists(KTX2Path.string())
                                               ? KTX2Path
                                               : TexturePath;
                        DecodeTasks.push_back({AbsolutePath,
                            FWorkStealThreadPool::Get()->EnqueueTask(
                                [DecodePath]() {
                                    return FAsyncTextureLoader::DecodeKTXTexture(DecodePath);
                                })});
                    }
                    else
                    {
                        DecodeTasks.push_back({AbsolutePath,
                            FWorkStealThreadPool::Get()->EnqueueTask(
                                [TexturePath]() {
                                    return FAsyncTextureLoader::DecodeSTBTexture(TexturePath);
                                })});
                    }
                }

                PathToTextures[AbsolutePath].push_back(OutTexture);
            }
        }
    }

    if (DecodeTasks.empty())
    {
        return 0;
    }

    // Batch upload on main thread
    nvrhi::CommandListHandle UploadCmdList = Device->createCommandList();
    UploadCmdList->open();

    uint32_t UniqueSuccessCount = 0;
    uint32_t MaterialRefSuccessCount = 0;
    for (auto& Task : DecodeTasks)
    {
        FDecodedImage Decoded = Task.Future.get();
        if (!Decoded.bIsValid)
        {
            HLVM_LOG(LogTexture, warn,
                TXT("LoadMaterialTexturesAsync: Failed to async-decode texture: {}"),
                *FString(Task.TexturePath.string().c_str()));
            continue;
        }

        // Upload once per unique path
        FTexture TempTexture;
        if (!UploadTextureToCommandList(Device, UploadCmdList, Decoded, TempTexture))
        {
            HLVM_LOG(LogTexture, err,
                TXT("LoadMaterialTexturesAsync: Failed to upload texture: {}"),
                *FString(Task.TexturePath.string().c_str()));
            continue;
        }

        nvrhi::TextureHandle Handle = TempTexture.GetTextureHandle();
        FTextureCache::Get().Insert(Task.TexturePath, Handle);
        ++UniqueSuccessCount;

        // Share the handle with all materials that reference this path
        auto It = PathToTextures.find(Task.TexturePath);
        if (It != PathToTextures.end())
        {
            for (FTexture* OutTexture : It->second)
            {
                if (OutTexture->InitializeFromHandle(Handle, Device))
                {
                    ++MaterialRefSuccessCount;
                }
            }
        }
    }

    UploadCmdList->close();
    Device->executeCommandList(UploadCmdList);
    Device->waitForIdle();

    HLVM_LOG(LogTexture, info,
        TXT("LoadMaterialTexturesAsync: Uploaded {}/{} unique textures, shared across {} material references"),
        UniqueSuccessCount, DecodeTasks.size(), MaterialRefSuccessCount);

    return UniqueSuccessCount;
}

// =====================================================================
// Non-blocking async load implementation
// =====================================================================

void FAsyncTextureLoader::BeginAsyncLoad(
    nvrhi::IDevice* Device,
    const TVector<std::shared_ptr<FPBRMaterial>>& Materials,
    const TVector<IMaterial::ETextureType>& TypesToLoad)
{
    TMap<FPath, TVector<FTexture*>> PathToTextures;
    TVector<FPendingTextureLoad> NewPending;

    for (auto& PBRMat : Materials)
    {
        if (!PBRMat)
            continue;

        for (IMaterial::ETextureType Type : TypesToLoad)
        {
            if (PBRMat->HasTexture(Type) && !PBRMat->HasGPUTexture(Type))
            {
                FPath TexturePath = PBRMat->GetTexturePath(Type);
                FPath AbsolutePath = FPath::Absolute(TexturePath);

                // Check texture cache first
                nvrhi::TextureHandle CachedTexture = FTextureCache::Get().GetTexture(AbsolutePath);
                if (CachedTexture)
                {
                    FTexture& OutTexture = PBRMat->GetGPUTexture(Type);
                    if (OutTexture.InitializeFromHandle(CachedTexture, Device))
                    {
                        continue;
                    }
                }

                FTexture* OutTexture = &PBRMat->GetGPUTexture(Type);

                if (!PathToTextures.contains(AbsolutePath))
                {
                    PathToTextures[AbsolutePath] = TVector<FTexture*>{};

                    FString Ext = FPath::GetExtension(TexturePath);
                    if (Ext == ".ktx" || Ext == ".KTX" ||
                        Ext == ".ktx2" || Ext == ".KTX2")
                    {
                        FPath KTX2Path = TexturePath.parent_path() / "ktx2" /
                                         (TexturePath.stem().string() + ".ktx2");
                        FPath DecodePath = std::filesystem::exists(KTX2Path.string())
                                               ? KTX2Path
                                               : TexturePath;
                        NewPending.push_back({AbsolutePath,
                            FWorkStealThreadPool::Get()->EnqueueTask(
                                [DecodePath]() {
                                    return FAsyncTextureLoader::DecodeKTXTexture(DecodePath);
                                }),
                            {}, Device});
                    }
                    else
                    {
                        NewPending.push_back({AbsolutePath,
                            FWorkStealThreadPool::Get()->EnqueueTask(
                                [TexturePath]() {
                                    return FAsyncTextureLoader::DecodeSTBTexture(TexturePath);
                                }),
                            {}, Device});
                    }
                }

                PathToTextures[AbsolutePath].push_back(OutTexture);
            }
        }
    }

    // Link target textures to pending loads
    for (auto& Pending : NewPending)
    {
        auto It = PathToTextures.find(Pending.TexturePath);
        if (It != PathToTextures.end())
        {
            Pending.TargetTextures = It->second;
        }
    }

    if (!NewPending.empty())
    {
        std::lock_guard<std::mutex> Lock(GPendingLoadsMutex);
        for (auto& Pending : NewPending)
        {
            GPendingLoads.push_back(std::move(Pending));
        }
        HLVM_LOG(LogTexture, info,
            TXT("BeginAsyncLoad: Enqueued {} texture decode tasks"),
            NewPending.size());
    }
}

uint32_t FAsyncTextureLoader::Poll(nvrhi::IDevice* Device)
{
    TVector<FPendingTextureLoad> Completed;
    {
        std::lock_guard<std::mutex> Lock(GPendingLoadsMutex);

        for (auto It = GPendingLoads.begin(); It != GPendingLoads.end(); )
        {
            if (It->Future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            {
                Completed.push_back(std::move(*It));
                It = GPendingLoads.erase(It);
            }
            else
            {
                ++It;
            }
        }
    }

    if (Completed.empty())
    {
        return 0;
    }

    // Collect valid decodes and create command list only when needed
    TVector<std::pair<FDecodedImage, FPendingTextureLoad>> ValidUploads;
    ValidUploads.reserve(Completed.size());

    for (auto& Task : Completed)
    {
        FDecodedImage Decoded = Task.Future.get();
        if (!Decoded.bIsValid)
        {
            HLVM_LOG(LogTexture, warn,
                TXT("Poll: Failed to async-decode texture: {}"),
                *FString(Task.TexturePath.string().c_str()));
            continue;
        }
        ValidUploads.emplace_back(std::move(Decoded), std::move(Task));
    }

    if (ValidUploads.empty())
    {
        return 0;
    }

    nvrhi::CommandListHandle UploadCmdList = Device->createCommandList();
    UploadCmdList->open();

    uint32_t UploadCount = 0;
    uint32_t RefCount = 0;

    for (auto& [Decoded, Task] : ValidUploads)
    {
        FTexture TempTexture;
        if (!UploadTextureToCommandList(Device, UploadCmdList, Decoded, TempTexture))
        {
            HLVM_LOG(LogTexture, err,
                TXT("Poll: Failed to upload texture: {}"),
                *FString(Task.TexturePath.string().c_str()));
            continue;
        }

        nvrhi::TextureHandle Handle = TempTexture.GetTextureHandle();
        FTextureCache::Get().Insert(Task.TexturePath, Handle);
        ++UploadCount;

        for (FTexture* OutTexture : Task.TargetTextures)
        {
            if (OutTexture->InitializeFromHandle(Handle, Device))
            {
                ++RefCount;
            }
        }
    }

    UploadCmdList->close();
    Device->executeCommandList(UploadCmdList);
    // NOTE: Intentionally NO waitForIdle() — upload command list executes
    // asynchronously. The render command list submitted later will see the
    // data because GPU execution is FIFO-ordered.

    if (UploadCount > 0)
    {
        HLVM_LOG(LogTexture, info,
            TXT("Poll: Uploaded {} textures ({} material refs), {} pending remaining"),
            UploadCount, RefCount, GPendingLoads.size());
    }

    return UploadCount;
}

bool FAsyncTextureLoader::HasPendingLoads()
{
    std::lock_guard<std::mutex> Lock(GPendingLoadsMutex);
    return !GPendingLoads.empty();
}
