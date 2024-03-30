/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Mallocator/VMMallocator/VMArena.h"
#include "Core/Mallocator/VMMallocator/SmallBinnedMallocator.h"
#include "Core/Assert.h"
#include "Template/ExpressionTemplate.tpp"
#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogVMArena)

FVMArena::FVMArena(const FVMArenaInitContext& _InitContext)
	: mInitCtx(_InitContext)
{
	/**
	 * Initialize heap chain, heaps are where we acquire memory from mimalloc and manage small/large allocations on our own
	 */
	HLVM_CONSTEXPR_ASSERT(bValidate, mInitCtx.Valid());
	auto _Heap = mHeapChainHead;
	for (size_t i = 0; i < mInitCtx.LargeHeapInitNum; ++i)
	{
		auto Heap = new (mOSPageMallocator.MallocSmall(sizeof(FHeapChain))) FHeapChain();
		Heap->HeapAllocator.Init(this, mInitCtx.LargeHeapSize);
		if (!mHeapChainHead)
		{
			_Heap = mHeapChainHead = Heap;
		}
		else
		{
			_Heap->Next = Heap;
			_Heap = Heap;
		}
		HLVM_LOG(LogVMArena, debug, TXT("VMArena: Initialized %d large heaps"), i + 1);
	}

	/**
	 * Intialize small binning allocators (using fancy compile time for-loop)
	 */
	ct_for<0, HLVM_VMA_SMALL_ALLOC_THRESHOLD / HLVM_VMA_SMALL_ALLOC_ALIGNMENT, 1, TUINT8>([&](auto i) {
		using BinnedMallocatorType = FSmallBinnedMallocator<(i.value + 1) * HLVM_VMA_SMALL_ALLOC_ALIGNMENT>;
		mSmallBinnedMallocators[i] = new (mOSPageMallocator.MallocSmall(sizeof(BinnedMallocatorType))) BinnedMallocatorType();
		mSmallBinnedMallocators[i]->Init(this);
	});
}

FVMArena::~FVMArena()
{
	/**
	 * Free all small binned allocators
	 */
	for (size_t i = 0; i < HLVM_VMA_SMALL_ALLOC_THRESHOLD / HLVM_VMA_SMALL_ALLOC_ALIGNMENT; ++i)
	{
		if (mSmallBinnedMallocators[i])
		{
			mSmallBinnedMallocators[i]->~ISmallBinnedMallocator();
			mOSPageMallocator.FreeSmall(mSmallBinnedMallocators[i]);
		}
	}
	/**
	 * Free all heaps
	 */
	while (mHeapChainHead)
	{
		auto Next = mHeapChainHead->Next;
		mHeapChainHead->~FHeapChain();
		mOSPageMallocator.FreeSmall(mHeapChainHead);
		mHeapChainHead = Next;
	}
}

void* FVMArena::Malloc(size_t size)
{
	return size <= HLVM_VMA_SMALL_ALLOC_THRESHOLD ? MallocBinned(size) : MallocHeap(size);
}

void FVMArena::Free(void* p)
{
	if (mOSPageMallocator.Owned(p))
	{
		if (auto alignment = FSmallBinnedBlockHead::IsSmallAlloc(p))
		{
			FreeBinned(p, alignment);
		}
		else
		{
			FreeHeap(p);
		}
	}
	else
	{
		// TODO, handle
	}
}

// TODO : TLS cached free list
void* FVMArena::MallocHeap(size_t size)
{
	HLVM_CONSTEXPR_ASSERT(bValidate, size > HLVM_VMA_SMALL_ALLOC_THRESHOLD);

	auto Heap = mHeapChainHead;
	while (Heap)
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
	Heap->Next = new (mOSPageMallocator.MallocSmall(sizeof(FHeapChain))) FHeapChain();
	auto& HeapMallocator = Heap->Next->HeapAllocator;
	auto  Capacity = FHeapMallocator::CalculateCapacity(size);
	if (Capacity <= mInitCtx.LargeHeapSize)
	{
		HeapMallocator.Init(this, mInitCtx.LargeHeapSize);
	}
	else
	{
		HeapMallocator.Init(this, size, true);
	}
	HLVM_CONSTEXPR_ASSERT(bValidate, HeapMallocator.GetHeapSize() >= size);
	auto p = HeapMallocator.Malloc(size);
	HLVM_CONSTEXPR_ASSERT(bValidate, p != nullptr);
	return p;
}

void FVMArena::FreeHeap(void* p)
{
	// TODO : use 32MB alignment to quickly find heap allocation instead of traversal
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
	// TODO, should use stack string assert
	assert(false);
}

void* FVMArena::MallocBinned(size_t _size)
{
	HLVM_CONSTEXPR_ASSERT(bValidate, _size <= HLVM_VMA_SMALL_ALLOC_THRESHOLD);
	TUINT8 size = FSmallBinnedBlockHead::GoodSize(_size);
	auto   p = mSmallBinnedMallocators[size / HLVM_VMA_SMALL_ALLOC_ALIGNMENT]->Malloc();
	HLVM_CONSTEXPR_ASSERT(bValidate, p != nullptr);
	return p;
}

void FVMArena::FreeBinned(void* p, TUINT8 size)
{
	HLVM_CONSTEXPR_ASSERT(bValidate, size <= HLVM_VMA_SMALL_ALLOC_THRESHOLD);
	mSmallBinnedMallocators[size / HLVM_VMA_SMALL_ALLOC_ALIGNMENT]->Free(p);
}

void* FVMArena::MallocOSPage(size_t, size_t)
{
	return mOSPageMallocator.MallocLargeHeap();
}

void FVMArena::FreeOSPage(void* p, size_t)
{
	mOSPageMallocator.FreeLargeHeap(p);
}

void* FVMArena::MallocLowLevel(size_t size)
{
	return GStdMallocator.Malloc(size);
}

void FVMArena::FreeLowLevel(void* p)
{
	GStdMallocator.Free(p);
}
