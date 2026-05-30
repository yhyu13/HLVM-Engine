// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/DescriptorTableManager.h"
#include "Core/Log.h"

#include <algorithm>

DECLARE_LOG_CATEGORY(LogRenderer)

FDescriptorHandle::FDescriptorHandle(std::shared_ptr<FDescriptorTableManager> Manager, FDescriptorIndex Index)
    : m_DescriptorIndex(Index)
    , m_Manager(Manager)
{
}

FDescriptorHandle::~FDescriptorHandle()
{
    if (m_DescriptorIndex >= 0)
    {
        auto Manager = m_Manager.lock();
        if (Manager)
        {
            Manager->ReleaseDescriptor(m_DescriptorIndex);
        }
        m_DescriptorIndex = -1;
    }
}

FDescriptorHandle::FDescriptorIndex FDescriptorHandle::GetHeapIndex() const
{
    if (m_DescriptorIndex >= 0)
    {
        auto Manager = m_Manager.lock();
        if (Manager)
        {
            uint32_t HeapBase = Manager->GetDescriptorTable()->getFirstDescriptorIndexInHeap();
            return static_cast<FDescriptorIndex>(HeapBase + static_cast<uint32_t>(m_DescriptorIndex));
        }
    }
    return -1;
}

void FDescriptorHandle::Reset()
{
    if (m_DescriptorIndex >= 0)
    {
        auto Manager = m_Manager.lock();
        if (Manager)
        {
            Manager->ReleaseDescriptor(m_DescriptorIndex);
        }
        m_DescriptorIndex = -1;
    }
    m_Manager.reset();
}

FDescriptorTableManager::FDescriptorTableManager(nvrhi::IDevice* Device, nvrhi::IBindingLayout* Layout)
    : m_Device(Device)
{
    m_DescriptorTable = m_Device->createDescriptorTable(Layout);

    uint32_t Capacity = m_DescriptorTable->getCapacity();
    m_AllocatedDescriptors.resize(Capacity);
    m_Descriptors.resize(Capacity);
    memset(m_Descriptors.data(), 0, sizeof(nvrhi::BindingSetItem) * Capacity);
}

FDescriptorTableManager::~FDescriptorTableManager()
{
    std::lock_guard<std::mutex> Lock(m_Mutex);
    for (auto& Descriptor : m_Descriptors)
    {
        if (Descriptor.resourceHandle)
        {
            Descriptor.resourceHandle->Release();
            Descriptor.resourceHandle = nullptr;
        }
    }
}

FDescriptorHandle::FDescriptorIndex FDescriptorTableManager::AllocateSlot()
{
    uint32_t Capacity = static_cast<uint32_t>(m_AllocatedDescriptors.size());

    // Search for free slot starting from m_SearchStart
    uint32_t StartSearch = static_cast<uint32_t>(m_SearchStart);
    for (uint32_t SearchIdx = StartSearch; SearchIdx < Capacity; SearchIdx++)
    {
        if (!m_AllocatedDescriptors[SearchIdx])
        {
            return static_cast<FDescriptorHandle::FDescriptorIndex>(SearchIdx);
        }
    }

    // No free slot found, need to resize
    return -1;
}

void FDescriptorTableManager::ResizeTable(uint32_t NewCapacity)
{
    uint32_t OldCapacity = static_cast<uint32_t>(m_AllocatedDescriptors.size());

    m_Device->resizeDescriptorTable(m_DescriptorTable, NewCapacity);
    m_AllocatedDescriptors.resize(NewCapacity);
    m_Descriptors.resize(NewCapacity);

    // Zero-fill the new descriptors
    memset(&m_Descriptors[OldCapacity], 0, sizeof(nvrhi::BindingSetItem) * (NewCapacity - OldCapacity));
}

FDescriptorHandle::FDescriptorIndex FDescriptorTableManager::CreateDescriptor(nvrhi::BindingSetItem Item)
{
    std::lock_guard<std::mutex> Lock(m_Mutex);

    // Check if we already have a descriptor for this resource (deduplication)
    const auto& Found = m_DescriptorIndexMap.find(Item);
    if (Found != m_DescriptorIndexMap.end())
    {
        return Found->second;
    }

    uint32_t Capacity = static_cast<uint32_t>(m_AllocatedDescriptors.size());
    FDescriptorHandle::FDescriptorIndex Index = AllocateSlot();

    if (Index < 0)
    {
        // No free slot, resize the table
        uint32_t NewCapacity = std::max(64u, Capacity * 2);
        ResizeTable(NewCapacity);
        // Allocate from the end
        Index = static_cast<FDescriptorHandle::FDescriptorIndex>(NewCapacity - 1);
    }

    // Set the slot index in the item (slot is uint32_t)
    Item.slot = static_cast<uint32_t>(Index);
    m_SearchStart = static_cast<int>(Index) + 1;
    m_AllocatedDescriptors[static_cast<size_t>(Index)] = true;
    m_Descriptors[static_cast<size_t>(Index)] = Item;
    m_DescriptorIndexMap[Item] = Index;

    // Write to GPU descriptor table
    m_Device->writeDescriptorTable(m_DescriptorTable, Item);

    // Add ref to the resource
    if (Item.resourceHandle)
    {
        Item.resourceHandle->AddRef();
    }

    return Index;
}

FDescriptorHandle FDescriptorTableManager::CreateDescriptorHandle(nvrhi::BindingSetItem Item)
{
    FDescriptorHandle::FDescriptorIndex Index = CreateDescriptor(Item);
    return FDescriptorHandle(shared_from_this(), Index);
}

nvrhi::BindingSetItem FDescriptorTableManager::GetDescriptor(FDescriptorHandle::FDescriptorIndex Index) const
{
    std::lock_guard<std::mutex> Lock(m_Mutex);
    if (Index < 0 || static_cast<size_t>(Index) >= m_Descriptors.size())
    {
        return nvrhi::BindingSetItem::None(0);
    }
    return m_Descriptors[static_cast<size_t>(Index)];
}

void FDescriptorTableManager::ReleaseDescriptor(FDescriptorHandle::FDescriptorIndex Index)
{
    std::lock_guard<std::mutex> Lock(m_Mutex);

    if (Index < 0 || static_cast<size_t>(Index) >= m_Descriptors.size())
    {
        return;
    }

    size_t VecIndex = static_cast<size_t>(Index);
    nvrhi::BindingSetItem& Descriptor = m_Descriptors[VecIndex];

    // Release the resource
    if (Descriptor.resourceHandle)
    {
        Descriptor.resourceHandle->Release();
        Descriptor.resourceHandle = nullptr;
    }

    // Remove from index map to prevent reuse
    const auto IndexMapEntry = m_DescriptorIndexMap.find(Descriptor);
    if (IndexMapEntry != m_DescriptorIndexMap.end())
    {
        m_DescriptorIndexMap.erase(IndexMapEntry);
    }

    // Mark slot as empty (None takes uint32_t)
    Descriptor = nvrhi::BindingSetItem::None(static_cast<uint32_t>(Index));

    // Write empty descriptor to GPU
    m_Device->writeDescriptorTable(m_DescriptorTable, Descriptor);

    m_AllocatedDescriptors[VecIndex] = false;
    m_SearchStart = std::min(m_SearchStart, static_cast<int>(Index));
}

bool FDescriptorTableManager::HasDescriptor(const nvrhi::BindingSetItem& Item) const
{
    std::lock_guard<std::mutex> Lock(m_Mutex);
    return m_DescriptorIndexMap.find(Item) != m_DescriptorIndexMap.end();
}

uint32_t FDescriptorTableManager::GetAllocatedCount() const
{
    std::lock_guard<std::mutex> Lock(m_Mutex);
    uint32_t Count = 0;
    for (bool Allocated : m_AllocatedDescriptors)
    {
        if (Allocated)
        {
            Count++;
        }
    }
    return Count;
}

nvrhi::BindingLayoutHandle FDescriptorTableManager::CreateTextureBindlessLayout(
    nvrhi::IDevice* Device,
    uint32_t MaxCapacity)
{
    nvrhi::BindlessLayoutDesc Desc;
    Desc.visibility = nvrhi::ShaderType::All;
    Desc.firstSlot = 0;
    Desc.maxCapacity = MaxCapacity;
    Desc.registerSpaces = {
        nvrhi::BindingLayoutItem::Texture_SRV(0),    // t0, t1, ... tN
        nvrhi::BindingLayoutItem::Sampler(1)          // s0, s1, ... sN (separate space)
    };

    return Device->createBindlessLayout(Desc);
}
