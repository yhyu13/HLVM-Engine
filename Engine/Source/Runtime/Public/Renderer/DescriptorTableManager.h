// Copyright 2026 HLVM Engine
//
// MIT License

#pragma once

#include "Core/String.h"

#include <nvrhi/nvrhi.h>

#include <unordered_map>
#include <memory>
#include <mutex>

/**
 * @brief RAII wrapper for a descriptor index in a bindless descriptor table
 *
 * Automatically releases the descriptor back to the table when destroyed.
 * This ensures no resource leaks even if the handle is abandoned.
 */
class FDescriptorHandle
{
public:
    using FDescriptorIndex = int;

    FDescriptorHandle() : m_DescriptorIndex(-1), m_Manager() {}
    FDescriptorHandle(std::shared_ptr<class FDescriptorTableManager> Manager, FDescriptorIndex Index);
    ~FDescriptorHandle();

    FDescriptorHandle(const FDescriptorHandle&) = delete;
    FDescriptorHandle(FDescriptorHandle&&) = default;
    FDescriptorHandle& operator=(const FDescriptorHandle&) = delete;
    FDescriptorHandle& operator=(FDescriptorHandle&&) = default;

    [[nodiscard]] bool IsValid() const { return m_DescriptorIndex >= 0 && !m_Manager.expired(); }
    [[nodiscard]] FDescriptorIndex Get() const { return m_DescriptorIndex; }

    // Get the index in the descriptor heap (volatile if table resizes)
    [[nodiscard]] FDescriptorIndex GetHeapIndex() const;

    void Reset();

private:
    FDescriptorIndex m_DescriptorIndex;
    std::weak_ptr<class FDescriptorTableManager> m_Manager;
};


/**
 * @brief Manages a bindless descriptor table for efficient resource access
 *
 * FDescriptorTableManager provides:
 * - Bindless resource allocation with automatic slot management
 * - Hash-based deduplication (same resource = same slot)
 * - Auto-resizing when capacity is reached
 * - RAII descriptor handles that auto-release on destruction
 *
 * This enables efficient access to many textures/buffers without creating
 * separate binding sets per resource.
 */
class FDescriptorTableManager : public std::enable_shared_from_this<FDescriptorTableManager>
{
public:
    /**
     * @brief Construct a descriptor table manager with a bindless layout
     * @param Device NVRHI device
     * @param Layout The bindless layout created via CreateBindlessLayout()
     */
    FDescriptorTableManager(nvrhi::IDevice* Device, nvrhi::IBindingLayout* Layout);
    ~FDescriptorTableManager();

    // Non-copyable
    FDescriptorTableManager(const FDescriptorTableManager&) = delete;
    FDescriptorTableManager& operator=(const FDescriptorTableManager&) = delete;

    /**
     * @brief Get the underlying descriptor table handle
     */
    [[nodiscard]] nvrhi::IDescriptorTable* GetDescriptorTable() const { return m_DescriptorTable; }

    /**
     * @brief Allocate a descriptor slot for a resource
     * @param Item The binding set item (resource to bind)
     * @return The allocated descriptor index, or -1 on failure
     */
    [[nodiscard]] FDescriptorHandle::FDescriptorIndex CreateDescriptor(nvrhi::BindingSetItem Item);

    /**
     * @brief Allocate a descriptor and get an RAII handle
     * @param Item The binding set item (resource to bind)
     * @return RAII handle that releases on destruction
     */
    [[nodiscard]] FDescriptorHandle CreateDescriptorHandle(nvrhi::BindingSetItem Item);

    /**
     * @brief Get a descriptor by index
     * @param Index Descriptor index
     * @return The binding set item at that index, or None if invalid
     */
    [[nodiscard]] nvrhi::BindingSetItem GetDescriptor(FDescriptorHandle::FDescriptorIndex Index) const;

    /**
     * @brief Release a descriptor by index (called by FDescriptorHandle destructor)
     * @param Index Descriptor index to release
     */
    void ReleaseDescriptor(FDescriptorHandle::FDescriptorIndex Index);

    /**
     * @brief Check if a descriptor exists for a resource
     * @param Item The binding set item to look up
     * @return true if a descriptor exists for this resource
     */
    [[nodiscard]] bool HasDescriptor(const nvrhi::BindingSetItem& Item) const;

    /**
     * @brief Get the total number of allocated descriptors
     */
    [[nodiscard]] uint32_t GetAllocatedCount() const;

    /**
     * @brief Create a standard bindless layout for textures
     * @param Device NVRHI device
     * @param MaxCapacity Maximum number of descriptors
     * @return The created bindless layout handle
     */
    static nvrhi::BindingLayoutHandle CreateTextureBindlessLayout(
        nvrhi::IDevice* Device,
        uint32_t MaxCapacity = 16384);

private:
    // Hash functor that ignores binding slot (only looks at resource identity)
    struct FBindingSetItemHasher
    {
        std::size_t operator()(const nvrhi::BindingSetItem& Item) const
        {
            size_t Hash = 0;
            nvrhi::hash_combine(Hash, Item.resourceHandle);
            nvrhi::hash_combine(Hash, Item.type);
            nvrhi::hash_combine(Hash, Item.format);
            nvrhi::hash_combine(Hash, Item.dimension);
            nvrhi::hash_combine(Hash, Item.rawData[0]);
            nvrhi::hash_combine(Hash, Item.rawData[1]);
            return Hash;
        }
    };

    // Equality functor that ignores binding slot
    struct FBindingSetItemsEqual
    {
        bool operator()(const nvrhi::BindingSetItem& A, const nvrhi::BindingSetItem& B) const
        {
            return A.resourceHandle == B.resourceHandle
                && A.type == B.type
                && A.format == B.format
                && A.dimension == B.dimension
                && A.subresources == B.subresources;
        }
    };

    nvrhi::DeviceHandle m_Device;
    nvrhi::DescriptorTableHandle m_DescriptorTable;

    mutable std::mutex m_Mutex;  // Protects all shared state
    std::vector<nvrhi::BindingSetItem> m_Descriptors;
    std::unordered_map<nvrhi::BindingSetItem, FDescriptorHandle::FDescriptorIndex, FBindingSetItemHasher, FBindingSetItemsEqual> m_DescriptorIndexMap;
    std::vector<bool> m_AllocatedDescriptors;
    int m_SearchStart = 0;

    FDescriptorHandle::FDescriptorIndex AllocateSlot();
    void ResizeTable(uint32_t NewCapacity);
};
