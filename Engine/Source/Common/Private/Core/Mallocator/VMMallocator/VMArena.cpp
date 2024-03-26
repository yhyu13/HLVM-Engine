/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Mallocator/VMMallocator/VMArena.h"
#include "Core/Mallocator/VMMallocator/SmallBinnedMallocator.h"
#include "Core/Assert.h"
#include "Template/ExpressionTemplate.tpp"

FVMArena::FVMArena(FMiMallocator* _MiMallocator,
	size_t						  _DefaultHeapSize,
	size_t						  _LargeHeapSize)
	: MiMallocator(_MiMallocator), mDefaultHeapSize(_DefaultHeapSize), mLargeHeapSize(_LargeHeapSize)
{
	/**
	 * Intialize small binning allocators (using fancy compile time for-loop)
	 */
	ct_for<0, HLVM_SMALL_ALLOC_THRESHOLD / HLVM_SMALL_ALLOC_ALIGNMENT, 1, TUINT8>([&](auto i) {
		using BinnedMallocatorType = FSmallBinnedMallocator<(i.value + 1) * HLVM_SMALL_ALLOC_ALIGNMENT>;
		mSmallBinnedMallocators[i] = new (MiMallocator->Malloc(sizeof(BinnedMallocatorType))) BinnedMallocatorType();
		mSmallBinnedMallocators[i]->Init(this);
	});
	/**
	 * Initialize heap chain, heaps are where we acquire memory from mimalloc and manage small/large allocations on our own
	 */
	mHeapChainHead = new (MiMallocator->Malloc(sizeof(FHeapChain))) FHeapChain();
	mHeapChainHead->HeapAllocator.Init(MiMallocator, mDefaultHeapSize);
}

FVMArena::~FVMArena()
{
	/**
	 * Free all heaps
	 */
	while (mHeapChainHead)
	{
		auto Next = mHeapChainHead->Next;
		mHeapChainHead->~FHeapChain();
		MiMallocator->Free(mHeapChainHead);
		mHeapChainHead = Next;
	}
}
// TODO
void* FVMArena::MallocHeap(size_t size)
{
	HLVM_CONSTEXPR_ASSERT(bValidate, size > HLVM_SMALL_ALLOC_THRESHOLD);

	auto Heap = mHeapChainHead;
	while (true)
	{
		// Try to allocate from the current managed heaps
		auto& HeapMallocator = Heap->HeapAllocator;
		if (HeapMallocator.Managed() && HeapMallocator.GetFreeBlockSizeUpperBound() >= size)
		{
			auto p = HeapMallocator.Malloc(size);
			HLVM_CONSTEXPR_ASSERT(bValidate, p != nullptr);
			return p;
		}
		if (!Heap->Next)
		{
			break;
		}
		Heap = Heap->Next;
	}

	// Allocate new heap space for this allocation since we don't have an available heap
	HLVM_CONSTEXPR_ASSERT(bValidate, Heap->Next == nullptr);
	Heap->Next = new (MiMallocator->Malloc(sizeof(FHeapChain))) FHeapChain();
	auto& HeapMallocator = Heap->Next->HeapAllocator;
	auto  Capacity = FHeapMallocator::CalculateCapacity(size);
	if (Capacity < mDefaultHeapSize)
	{
		HeapMallocator.Init(MiMallocator, mDefaultHeapSize);
	}
	else if (Capacity < mLargeHeapSize)
	{
		HeapMallocator.Init(MiMallocator, mLargeHeapSize);
	}
	else
	{
		HeapMallocator.Init(MiMallocator, Capacity);
	}
	HLVM_CONSTEXPR_ASSERT(bValidate, HeapMallocator.GetHeapSize() >= Capacity);
	auto p = HeapMallocator.Malloc(size);
	HLVM_CONSTEXPR_ASSERT(bValidate, p != nullptr);
	return p;
}

void* FVMArena::MallocBinned(size_t _size)
{
	HLVM_CONSTEXPR_ASSERT(bValidate, _size <= HLVM_SMALL_ALLOC_THRESHOLD);
	TUINT8 size = FSmallBinnedBlockHead::GoodSize(_size);
	auto   p = mSmallBinnedMallocators[size / HLVM_SMALL_ALLOC_ALIGNMENT]->Malloc();
	HLVM_CONSTEXPR_ASSERT(bValidate, p != nullptr);
	return p;
}

void FVMArena::FreeHeap(void* p)
{
	auto Heap = mHeapChainHead;
	while (Heap)
	{
		// Try to allocate from the current managed heaps
		auto& HeapMallocator = Heap->HeapAllocator;
		if (HeapMallocator.Owned(p))
		{
			HeapMallocator.Free(p);
			return;
		}
		Heap = Heap->Next;
	}
	MiMallocator->Free(p);
}

void FVMArena::FreeBinned(void* p, TUINT8 size)
{
	HLVM_CONSTEXPR_ASSERT(bValidate, size <= HLVM_SMALL_ALLOC_THRESHOLD);
	mSmallBinnedMallocators[size / HLVM_SMALL_ALLOC_ALIGNMENT]->Free(p);
}
