/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Mallocator/VMMallocator/VMArena.h"
#include "Core/Assert.h"

FVMArena::FVMArena(FMiMallocator* _MiMallocator,
	size_t						  _DefaultHeapSize,
	size_t						  _LargeHeapSize)
	: MiMallocator(_MiMallocator), mDefaultHeapSize(_DefaultHeapSize), mLargeHeapSize(_LargeHeapSize)
{
	for (uint8_t i = 0; i < HLVM_SMALL_ALLOC_THRESHOLD / HLVM_SMALL_ALLOC_ALIGNMENT; ++i)
	{
		mSmallBinnedMallocators[i].Init(this, (i + 1) * HLVM_SMALL_ALLOC_ALIGNMENT);
	}
	mHeapChainHead = new (MiMallocator->Malloc(sizeof(FHeapChain))) FHeapChain();
	mHeapChainHead->HeapAllocator.Init(MiMallocator, mDefaultHeapSize);
}

FVMArena::~FVMArena()
{
	while (mHeapChainHead)
	{
		auto Next = mHeapChainHead->Next;
		mHeapChainHead->~FHeapChain();
		MiMallocator->Free(mHeapChainHead);
		mHeapChainHead = Next;
	}
}
// TODO
void* FVMArena::Malloc(size_t size)
{
	HLVM_CONSTEXPR_ASSERT(bValidate, size > HLVM_SMALL_ALLOC_THRESHOLD);

	auto Heap = mHeapChainHead;
	while (1)
	{
		// Try to allocate from the current managed heaps
		auto& HeapMallocator = Heap->HeapAllocator;
		if (HeapMallocator.Managed() && HeapMallocator.GetFreeSizeUpperBound() >= size)
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
	{
		// Allocate new heap space for this allocation
		HLVM_CONSTEXPR_ASSERT(bValidate, Heap->Next == nullptr);
		Heap->Next = new (MiMallocator->Malloc(sizeof(FHeapChain))) FHeapChain();
		auto& HeapMallocator = Heap->Next->HeapAllocator;
		auto  NewSize = size + FHeapMallocator::GetHeaderSize();
		if (NewSize < mDefaultHeapSize)
		{
			HeapMallocator.Init(MiMallocator, mDefaultHeapSize);
		}
		else if (NewSize < mLargeHeapSize)
		{
			HeapMallocator.Init(MiMallocator, mLargeHeapSize);
		}
		else
		{
			HeapMallocator.Init(MiMallocator, NewSize);
		}
		HLVM_CONSTEXPR_ASSERT(bValidate, HeapMallocator.GetEffectiveSize() > NewSize);
		auto p = HeapMallocator.Malloc(size);
		HLVM_CONSTEXPR_ASSERT(bValidate, p != nullptr);
		return p;
	}
}
void* FVMArena::MallocSmall(size_t _size)
{
#if HLVM_MALLOC_VALIDATION
	assert(_size <= HLVM_SMALL_ALLOC_THRESHOLD);
#endif
	uint8_t size = FSmallBinnedBlockHead::GoodSize(_size);
	return mSmallBinnedMallocators[size / HLVM_SMALL_ALLOC_ALIGNMENT].Malloc();
}
// TODO
void FVMArena::Free(void* p)
{
	(void)p;
}
void FVMArena::FreeSmall(void* p, uint8_t size)
{
	mSmallBinnedMallocators[size / HLVM_SMALL_ALLOC_ALIGNMENT].Free(p);
}
