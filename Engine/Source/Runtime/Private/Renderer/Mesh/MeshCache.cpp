// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/Mesh/MeshCache.h"
#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogMeshCache)

size_t FMeshCache::FMeshKeyHash::operator()(const FMeshKey& Key) const noexcept
{
    size_t Hash1 = std::hash<FPath>{}(Key.ScenePath);
    size_t Hash2 = std::hash<uint32_t>{}(Key.MeshIndex);
    return Hash1 ^ (Hash2 + 0x9e3779b9 + (Hash1 << 6) + (Hash1 >> 2));
}

const FMeshCache::FMeshEntry* FMeshCache::Find(const FMeshKey& Key) const
{
    auto It = Cache.find(Key);
    if (It != Cache.end())
    {
        return &It->second;
    }
    return nullptr;
}

void FMeshCache::Insert(const FMeshKey& Key, const FMeshEntry& Entry)
{
    Cache[Key] = Entry;

    if (MemoryBudget)
    {
        size_t VertexBytes = Entry.VertexBuffer ? Entry.VertexBuffer->getDesc().byteSize : 0;
        size_t IndexBytes = Entry.IndexBuffer ? Entry.IndexBuffer->getDesc().byteSize : 0;
        if (!MemoryBudget->TryAllocate(VertexBytes + IndexBytes))
        {
            HLVM_LOG(LogMeshCache, warn,
                TXT("FMeshCache: Memory budget exceeded — inserted mesh {} anyway (advisory only)"),
                Key.MeshIndex);
        }
    }

    HLVM_LOG(LogMeshCache, info, TXT("FMeshCache: Inserted mesh {} from {}"),
        Key.MeshIndex, Key.ScenePath.ToTCharCStr());
}

void FMeshCache::Clear()
{
    if (MemoryBudget)
    {
        for (const auto& Pair : Cache)
        {
            size_t VertexBytes = Pair.second.VertexBuffer ? Pair.second.VertexBuffer->getDesc().byteSize : 0;
            size_t IndexBytes = Pair.second.IndexBuffer ? Pair.second.IndexBuffer->getDesc().byteSize : 0;
            MemoryBudget->Free(VertexBytes + IndexBytes);
        }
    }
    Cache.clear();
}

uint32_t FMeshCache::GetNumEntries() const
{
    return static_cast<uint32_t>(Cache.size());
}

size_t FMeshCache::GetTotalVertexBytes() const
{
    size_t Total = 0;
    for (const auto& Pair : Cache)
    {
        if (Pair.second.VertexBuffer)
        {
            Total += Pair.second.VertexBuffer->getDesc().byteSize;
        }
    }
    return Total;
}

size_t FMeshCache::GetTotalIndexBytes() const
{
    size_t Total = 0;
    for (const auto& Pair : Cache)
    {
        if (Pair.second.IndexBuffer)
        {
            Total += Pair.second.IndexBuffer->getDesc().byteSize;
        }
    }
    return Total;
}
