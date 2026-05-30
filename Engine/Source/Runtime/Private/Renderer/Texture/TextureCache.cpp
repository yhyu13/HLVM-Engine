// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/Texture/TextureCache.h"
#include "Core/Log.h"
#include "Platform/FileSystem/FileSystem.h"
#include "Renderer/DescriptorTableManager.h"

#include <boost/filesystem.hpp>

DECLARE_LOG_CATEGORY(LogTextureCache)

size_t FTextureCache::EstimateTextureMemory(const nvrhi::TextureDesc& Desc)
{
    uint32_t BytesPerPixel = 4; // Default to 4 bytes (RGBA8)

    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wswitch-enum"
    switch (Desc.format)
    {
        case nvrhi::Format::R8_UNORM:
        case nvrhi::Format::R8_UINT:
        case nvrhi::Format::R8_SINT:
            BytesPerPixel = 1;
            break;
        case nvrhi::Format::RG8_UNORM:
        case nvrhi::Format::RG8_UINT:
        case nvrhi::Format::RG8_SINT:
        case nvrhi::Format::R16_FLOAT:
        case nvrhi::Format::R16_UNORM:
        case nvrhi::Format::R16_UINT:
        case nvrhi::Format::R16_SINT:
        case nvrhi::Format::D16:
            BytesPerPixel = 2;
            break;
        case nvrhi::Format::RGBA8_UNORM:
        case nvrhi::Format::RGBA8_SNORM:
        case nvrhi::Format::RGBA8_UINT:
        case nvrhi::Format::RGBA8_SINT:
        case nvrhi::Format::BGRA8_UNORM:
        case nvrhi::Format::SRGBA8_UNORM:
        case nvrhi::Format::R10G10B10A2_UNORM:
        case nvrhi::Format::R11G11B10_FLOAT:
        case nvrhi::Format::R32_UINT:
        case nvrhi::Format::R32_SINT:
        case nvrhi::Format::R32_FLOAT:
        case nvrhi::Format::D24S8:
        case nvrhi::Format::D32:
            BytesPerPixel = 4;
            break;
        case nvrhi::Format::RGBA16_FLOAT:
        case nvrhi::Format::RGBA16_UNORM:
        case nvrhi::Format::RGBA16_UINT:
        case nvrhi::Format::RGBA16_SINT:
        case nvrhi::Format::RG32_FLOAT:
        case nvrhi::Format::RG32_UINT:
        case nvrhi::Format::RG32_SINT:
            BytesPerPixel = 8;
            break;
        case nvrhi::Format::RGBA32_FLOAT:
        case nvrhi::Format::RGBA32_UINT:
        case nvrhi::Format::RGBA32_SINT:
            BytesPerPixel = 16;
            break;
        default:
            BytesPerPixel = 4;
            break;
    }
    #pragma clang diagnostic pop

    // Base size for mip 0
    size_t BaseSize = static_cast<size_t>(Desc.width) * Desc.height * BytesPerPixel;

    // Mip chain adds ~33% more
    if (Desc.mipLevels > 1)
    {
        BaseSize = static_cast<size_t>(static_cast<double>(BaseSize) * 1.33);
    }

    return BaseSize;
}

nvrhi::TextureHandle FTextureCache::GetTexture(const FPath& FilePath) const
{
    FPath AbsolutePath = FPath::Absolute(FilePath);

    LOCK_GUARD_NC();
    auto It = Cache.find(AbsolutePath);
    if (It != Cache.end())
    {
        return It->second.Texture;
    }

    return nullptr;
}

void FTextureCache::Insert(const FPath& FilePath, nvrhi::TextureHandle Texture)
{
    if (!Texture)
    {
        return;
    }

    FPath AbsolutePath = FPath::Absolute(FilePath);

    FEntry Entry;
    Entry.Texture = Texture;
    Entry.MemoryBytes = EstimateTextureMemory(Texture->getDesc());
    Entry.BindlessIndex = -1;  // Default: no bindless slot

    // Allocate bindless slot if manager is set
    if (DescriptorTableManager)
    {
        nvrhi::BindingSetItem Item = nvrhi::BindingSetItem::Texture_SRV(0, Texture);
        Entry.BindlessIndex = DescriptorTableManager->CreateDescriptor(Item);
        HLVM_LOG(LogTextureCache, info, TXT("FTextureCache: Allocated bindless slot {} for {}"),
            Entry.BindlessIndex, AbsolutePath.ToTCharCStr());
    }

    try
    {
        Entry.LastWriteTime = boost::filesystem::last_write_time(AbsolutePath.string());
    }
    catch (const boost::filesystem::filesystem_error&)
    {
        Entry.LastWriteTime = 0;
    }

    {
        LOCK_GUARD_NC();
        Cache[AbsolutePath] = Entry;
    }

    if (MemoryBudget)
    {
        MemoryBudget->TryAllocate(Entry.MemoryBytes);
    }

    HLVM_LOG(LogTextureCache, info, TXT("FTextureCache: Inserted {} ({} bytes)"), AbsolutePath.ToTCharCStr(), Entry.MemoryBytes);
}

void FTextureCache::Invalidate(const FPath& FilePath)
{
    FPath AbsolutePath = FPath::Absolute(FilePath);

    LOCK_GUARD_NC();
    auto It = Cache.find(AbsolutePath);
    if (It != Cache.end())
    {
        if (MemoryBudget)
        {
            MemoryBudget->Free(It->second.MemoryBytes);
        }
        HLVM_LOG(LogTextureCache, info, TXT("FTextureCache: Invalidated {}"), AbsolutePath.ToTCharCStr());
        Cache.erase(It);
    }
}

void FTextureCache::Clear()
{
    LOCK_GUARD_NC();
    if (MemoryBudget)
    {
        for (const auto& Pair : Cache)
        {
            MemoryBudget->Free(Pair.second.MemoryBytes);
        }
    }
    Cache.clear();
}

uint32_t FTextureCache::GetNumEntries() const
{
    LOCK_GUARD_NC();
    return static_cast<uint32_t>(Cache.size());
}

size_t FTextureCache::GetTotalMemoryBytes() const
{
    LOCK_GUARD_NC();
    size_t Total = 0;
    for (const auto& Pair : Cache)
    {
        Total += Pair.second.MemoryBytes;
    }
    return Total;
}

void FTextureCache::SetDescriptorTableManager(FDescriptorTableManager* Manager)
{
    LOCK_GUARD_NC();
    DescriptorTableManager = Manager;
}

FTextureCache::FBindlessIndex FTextureCache::GetBindlessIndex(const FPath& FilePath) const
{
    FPath AbsolutePath = FPath::Absolute(FilePath);

    LOCK_GUARD_NC();
    auto It = Cache.find(AbsolutePath);
    if (It != Cache.end())
    {
        return It->second.BindlessIndex;
    }

    return -1;
}

void FTextureCache::DrawUI()
{
#ifndef HLVM_BUILD_RELEASE
    if (ImGui::Begin("Texture Cache"))
    {
        LOCK_GUARD_NC();

        ImGui::Text("Entries: %u", static_cast<uint32_t>(Cache.size()));
        ImGui::Text("Estimated Memory: %.2f MB", GetTotalMemoryBytes() / (1024.0f * 1024.0f));
        ImGui::Text("Bindless Mode: %s", IsBindlessEnabled() ? "Enabled" : "Disabled");
        ImGui::Separator();

        if (ImGui::BeginTable("TextureCacheTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Path");
            ImGui::TableSetupColumn("Size");
            ImGui::TableSetupColumn("Format");
            ImGui::TableSetupColumn("Memory");
            ImGui::TableSetupColumn("Bindless");
            ImGui::TableHeadersRow();

            for (const auto& Pair : Cache)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", CHARSTR(Pair.first.string().c_str()));

                ImGui::TableNextColumn();
                if (Pair.second.Texture)
                {
                    const auto& Desc = Pair.second.Texture->getDesc();
                    ImGui::Text("%dx%d", Desc.width, Desc.height);
                }
                else
                {
                    ImGui::Text("N/A");
                }

                ImGui::TableNextColumn();
                if (Pair.second.Texture)
                {
                    const auto& Desc = Pair.second.Texture->getDesc();
                    ImGui::Text("%d", static_cast<int>(Desc.format));
                }
                else
                {
                    ImGui::Text("N/A");
                }

                ImGui::TableNextColumn();
                ImGui::Text("%.2f KB", Pair.second.MemoryBytes / 1024.0f);

                ImGui::TableNextColumn();
                if (Pair.second.BindlessIndex >= 0)
                {
                    ImGui::Text("%d", Pair.second.BindlessIndex);
                }
                else
                {
                    ImGui::Text("-");
                }
            }

            ImGui::EndTable();
        }
    }
    ImGui::End();
#endif
}
