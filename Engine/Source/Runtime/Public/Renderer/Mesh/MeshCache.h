// Copyright 2026 HLVM Engine
//
// MIT License

#pragma once

#include "Core/String.h"
#include "Platform/FileSystem/Path.h"
#include "Renderer/Utility/FMemoryBudget.h"
#include <nvrhi/nvrhi.h>
#include <parallel_hashmap/phmap.h>

/**
 * @brief Deduplicates vertex/index buffers across scene loads
 *
 * When the same scene is loaded multiple times (or different scenes share
 * the same mesh file), FMeshCache prevents redundant VB/IB creation.
 *
 * Key: (ScenePath, MeshIndex) — assumes meshes are stable within a file.
 * Value: Cached buffers + metadata.
 */
class FMeshCache
{
public:
    struct FMeshKey
    {
        FPath ScenePath;
        uint32_t MeshIndex;

        bool operator==(const FMeshKey& Other) const noexcept
        {
            return ScenePath == Other.ScenePath && MeshIndex == Other.MeshIndex;
        }
    };

    struct FMeshKeyHash
    {
        size_t operator()(const FMeshKey& Key) const noexcept;
    };

    struct FMeshEntry
    {
        nvrhi::BufferHandle VertexBuffer;
        nvrhi::BufferHandle IndexBuffer;
        uint32_t IndexCount = 0;
        uint32_t VertexCount = 0;
    };

    // Look up existing entry or return nullptr
    const FMeshEntry* Find(const FMeshKey& Key) const;

    // Insert a newly created entry
    void Insert(const FMeshKey& Key, const FMeshEntry& Entry);

    // Release all cached buffers
    void Clear();

    // Stats
    uint32_t GetNumEntries() const;
    size_t GetTotalVertexBytes() const;
    size_t GetTotalIndexBytes() const;

    // Memory budget integration
    void SetMemoryBudget(FMemoryBudget* InBudget) { MemoryBudget = InBudget; }

private:
    phmap::node_hash_map<FMeshKey, FMeshEntry, FMeshKeyHash> Cache;
    FMemoryBudget* MemoryBudget = nullptr;
};
