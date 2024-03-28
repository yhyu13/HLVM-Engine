/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Mallocator/VMMallocator/VMArena.h"
#include "Core/Mallocator/VMMallocator/SmallBinnedMallocator.h"
#include "Core/Assert.h"
#include "Template/ExpressionTemplate.tpp"

DECLARE_LOG_CATEGORY(LogVMArena)

FVMArena::FVMArena(FMiMallocator* _MiMallocator, const FVMArenaInitContext& _InitContext)
	: MiMallocator(_MiMallocator), mInitCtx(_InitContext)
{
	/**
	 * Initialize heap chain, heaps are where we acquire memory from mimalloc and manage small/large allocations on our own
	 */
	HLVM_CONSTEXPR_ASSERT(bValidate, mInitCtx.Valid());
	auto _Heap = mHeapChainHead;
	for (size_t i = 0; i < mInitCtx.LargeHeapInitNum; ++i)
	{
		auto Heap = new (MiMallocator->Malloc(sizeof(FHeapChain))) FHeapChain();
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
	for (size_t i = 0; i < mInitCtx.DefaultHeapInitNum; ++i)
	{
		auto Heap = new (MiMallocator->Malloc(sizeof(FHeapChain))) FHeapChain();
		Heap->HeapAllocator.Init(this, mInitCtx.DefaultHeapSize);
		if (!mHeapChainHead)
		{
			_Heap = mHeapChainHead = Heap;
		}
		else
		{
			_Heap->Next = Heap;
			_Heap = Heap;
		}
		HLVM_LOG(LogVMArena, debug, TXT("VMArena: Initialized %d default heaps"), i + 1);
	}

	/**
	 * Intialize small binning allocators (using fancy compile time for-loop)
	 */
	ct_for<0, HLVM_SMALL_ALLOC_THRESHOLD / HLVM_SMALL_ALLOC_ALIGNMENT, 1, TUINT8>([&](auto i) {
		using BinnedMallocatorType = FSmallBinnedMallocator<(i.value + 1) * HLVM_SMALL_ALLOC_ALIGNMENT>;
		mSmallBinnedMallocators[i] = new (MiMallocator->Malloc(sizeof(BinnedMallocatorType))) BinnedMallocatorType();
		mSmallBinnedMallocators[i]->Init(this);
	});
}

FVMArena::~FVMArena()
{
	/**
	 * Free all small binned allocators
	 */
	for (size_t i = 0; i < HLVM_SMALL_ALLOC_THRESHOLD / HLVM_SMALL_ALLOC_ALIGNMENT; ++i)
	{
		if (mSmallBinnedMallocators[i])
		{
			mSmallBinnedMallocators[i]->~ISmallBinnedMallocator();
			MiMallocator->Free(mSmallBinnedMallocators[i]);
		}
	}
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
// TODO : TLS cached free list
void* FVMArena::MallocHeap(size_t size)
{
	HLVM_CONSTEXPR_ASSERT(bValidate, size > HLVM_SMALL_ALLOC_THRESHOLD);

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
	Heap->Next = new (MiMallocator->Malloc(sizeof(FHeapChain))) FHeapChain();
	auto& HeapMallocator = Heap->Next->HeapAllocator;
	auto  Capacity = FHeapMallocator::CalculateCapacity(size);
	if (Capacity <= mInitCtx.DefaultHeapSize)
	{
		HeapMallocator.Init(this, mInitCtx.DefaultHeapSize);
	}
	else if (Capacity <= mInitCtx.LargeHeapSize)
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

void* FVMArena::MallocBinned(size_t _size)
{
	HLVM_CONSTEXPR_ASSERT(bValidate, _size <= HLVM_SMALL_ALLOC_THRESHOLD);
	TUINT8 size = FSmallBinnedBlockHead::GoodSize(_size);
	auto   p = mSmallBinnedMallocators[size / HLVM_SMALL_ALLOC_ALIGNMENT]->Malloc();
	HLVM_CONSTEXPR_ASSERT(bValidate, p != nullptr);
	return p;
}

void FVMArena::FreeBinned(void* p, TUINT8 size)
{
	HLVM_CONSTEXPR_ASSERT(bValidate, size <= HLVM_SMALL_ALLOC_THRESHOLD);
	mSmallBinnedMallocators[size / HLVM_SMALL_ALLOC_ALIGNMENT]->Free(p);
}

void* FVMArena::MallocOS(size_t size, size_t alignment)
{
	return MiMallocator->MallocAligned(size, alignment);
}

void FVMArena::FreeOS(void* p)
{
	MiMallocator->Free(p);
}
