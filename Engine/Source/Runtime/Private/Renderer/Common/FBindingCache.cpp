/**
 * HLVM-Engine: Binding cache implementation
 */

#include "Renderer/Common/FBindingCache.h"

#include <cstdint>
#include <utility>
#include <unordered_map>
#include <shared_mutex>

// Hash combine utility - combines hash values
static size_t HashCombine(size_t Lhs, size_t Rhs)
{
    return Lhs ^ (Rhs + 0x9e3779b9 + (Lhs << 6) + (Lhs >> 2));
}

// Hash a single binding item based on its key properties
static size_t HashBindingItem(const nvrhi::BindingSetItem& Item)
{
    size_t Hash = reinterpret_cast<size_t>(Item.resourceHandle);
    Hash = HashCombine(Hash, static_cast<size_t>(Item.slot));
    Hash = HashCombine(Hash, static_cast<size_t>(Item.type));
    Hash = HashCombine(Hash, static_cast<size_t>(Item.format));
    Hash = HashCombine(Hash, Item.rawData[0]);
    Hash = HashCombine(Hash, Item.rawData[1]);
    return Hash;
}

// Compute hash for a BindingSetDesc
static size_t ComputeBindingSetDescHash(const nvrhi::BindingSetDesc& Desc)
{
    size_t Hash = Desc.bindings.size();
    
    for (size_t Index = 0; Index < Desc.bindings.size(); ++Index)
    {
        const nvrhi::BindingSetItem& Item = Desc.bindings[Index];
        Hash = HashCombine(Hash, HashBindingItem(Item));
    }
    
    return Hash;
}

nvrhi::BindingSetHandle FBindingCache::GetCachedBindingSet(
    const nvrhi::BindingSetDesc& Desc,
    nvrhi::IBindingLayout* Layout)
{
    (void)Layout; // Unused in lookup, only needed for creation
    const size_t Hash = ComputeBindingSetDescHash(Desc);
    
    std::shared_lock<std::shared_mutex> ReadLock(Mutex);
    
    auto It = BindingSets.find(Hash);
    if (It != BindingSets.end())
    {
        return It->second;
    }
    
    return nullptr;
}

nvrhi::BindingSetHandle FBindingCache::GetOrCreateBindingSet(
    const nvrhi::BindingSetDesc& Desc,
    nvrhi::IBindingLayout* Layout)
{
    const size_t Hash = ComputeBindingSetDescHash(Desc);
    
    // First try to find existing binding set with read lock
    {
        std::shared_lock<std::shared_mutex> ReadLock(Mutex);
        
        auto It = BindingSets.find(Hash);
        if (It != BindingSets.end())
        {
            return It->second;
        }
    }
    
    // Not found, create new one with write lock
    std::unique_lock<std::shared_mutex> WriteLock(Mutex);
    
    // Double-check after acquiring write lock (another thread may have created it)
    auto It = BindingSets.find(Hash);
    if (It != BindingSets.end())
    {
        return It->second;
    }
    
    // Create new binding set
    nvrhi::BindingSetDesc DescCopy = Desc;
    nvrhi::BindingSetHandle BindingSet = Device->createBindingSet(DescCopy, Layout);
    
    if (BindingSet)
    {
        BindingSets[Hash] = BindingSet;
    }
    
    return BindingSet;
}

void FBindingCache::Clear()
{
    std::unique_lock<std::shared_mutex> WriteLock(Mutex);
    BindingSets.clear();
}
