// Copyright 2026 HLVM Engine
//
// MIT License

#pragma once

#include "Core/String.h"
#include "Core/Parallel/Lock.h"
#include "Platform/FileSystem/Path.h"
#include "Renderer/Utility/FMemoryBudget.h"

#include <nvrhi/nvrhi.h>

#include <ctime>

/**
 * @brief Centralized texture cache for GPU texture deduplication
 *
 * FTextureCache provides:
 * - Path-based deduplication: same texture file loaded once, shared across materials
 * - NVRHI handle storage: ref-counted handles automatically manage GPU memory
 * - Memory tracking: estimated VRAM usage per entry and total
 * - File modification tracking: for future hot-reload support
 *
 * Thread-safe for concurrent reads. All texture loads happen on main thread
 * in practice; lock protects against future async usage.
 */
class FTextureCache : private FAtomicFlagNC
{
public:
    FTextureCache() = default;

    /**
     * @brief Get cached texture or nullptr if not loaded
     */
    nvrhi::TextureHandle GetTexture(const FPath& FilePath) const;

    /**
     * @brief Insert a texture into the cache
     *
     * Should be called after a texture is decoded and uploaded.
     * If an entry already exists for this path, it is replaced.
     */
    void Insert(const FPath& FilePath, nvrhi::TextureHandle Texture);

    /**
     * @brief Remove a cached entry
     */
    void Invalidate(const FPath& FilePath);

    /**
     * @brief Clear all cached entries
     */
    void Clear();

    /**
     * @brief Get number of cached entries
     */
    uint32_t GetNumEntries() const;

    /**
     * @brief Get estimated total memory of all cached textures
     */
    size_t GetTotalMemoryBytes() const;

    /**
     * @brief ImGui debug UI showing cache entries and stats
     */
    void DrawUI();

    // Memory budget integration
    void SetMemoryBudget(FMemoryBudget* InBudget) { MemoryBudget = InBudget; }

private:
    struct FEntry
    {
        nvrhi::TextureHandle Texture;
        std::time_t LastWriteTime = 0;
        size_t MemoryBytes = 0;
    };

    static size_t EstimateTextureMemory(const nvrhi::TextureDesc& Desc);

    mutable TMap<FPath, FEntry> Cache;
    FMemoryBudget* MemoryBudget = nullptr;
};
