/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Mallocator/VMMallocator/VMArena.h"
#include "Core/Mallocator/VMMallocator/SmallBinnedMallocator.h"
#include "Core/Assert.h"
#include "Core/Log.h"

#include "Template/ExpressionTemplate.tpp"

DECLARE_LOG_CATEGORY(LogVMArena)

FVMArena::FVMArena()
{
	{
		auto _Heap = mHeapChainHead;
		// Init at least one large heap
		for (size_t i = 0; i < 1; ++i)
		{
			auto Heap = std::construct_at(R_C(FVMHeapChain*, mOSPageMallocator.MallocSmall(sizeof(FVMHeapChain))));
			Heap->HeapAllocator.Init(this, HLVM_VMA_LARGE_HEAP_SIZE);
			if (!mHeapChainHead)
			{
				// Set head if null
				_Heap = mHeapChainHead = Heap;
			}
			else
			{
				// Set next
				_Heap->Next = Heap;
				_Heap = Heap;
			}
			// HLVM_LOG(LogVMArena, debug, TXT("VMArena: Initialized {} large heaps"), i + 1);
		}
	}

	/**
	 * Intialize small binning allocators (using fancy compile time for-loop)
	 */
	ct_for<0, HLVM_VMA_SMALL_ALLOC_THRESHOLD / HLVM_VMA_SMALL_ALLOC_ALIGNMENT, 1, TUINT8>([&](auto i) {
		// Initialize binned allocator
		using BinnedMallocatorType = FSmallBinnedMallocator<(i.value + 1) * HLVM_VMA_SMALL_ALLOC_ALIGNMENT>;
		mSmallBinnedMallocators[i] = std::construct_at(R_C(BinnedMallocatorType*, mOSPageMallocator.MallocSmall(sizeof(BinnedMallocatorType))));
		mSmallBinnedMallocators[i]->Init(this);
		// HLVM_LOG(LogVMArena, debug, TXT("VMArena: Initialized {} SmallBinnedMallocator"), i + 1);
	});
}

FVMArena::~FVMArena()
{
	/**
	 * Handle cache free list
	 */
	while (mPendingFressLists.GenericFreeList.size())
	{
		auto LastGenericPtr = mPendingFressLists.GenericFreeList.back();
		if (!Owned(LastGenericPtr))
		{
			sGlobalPendingFreeList.Push({ .ptrToBeFree = LastGenericPtr, .tidNotOwned = GCurrentTID64 });
		}
		mPendingFressLists.GenericFreeList.pop_back();
	}
	while (mPendingFressLists.NonLocalFreeList.size())
	{
		auto LastNonLocalPtr = mPendingFressLists.NonLocalFreeList.back();
		sGlobalPendingFreeList.Push({ .ptrToBeFree = LastNonLocalPtr, .tidNotOwned = GCurrentTID64 });
		mPendingFressLists.NonLocalFreeList.pop_back();
	}

	/**
	 * Free all small binned allocators
	 */
	for (size_t i = 0; i < HLVM_VMA_SMALL_ALLOC_THRESHOLD / HLVM_VMA_SMALL_ALLOC_ALIGNMENT; ++i)
	{
		if (mSmallBinnedMallocators[i])
		{
			std::destroy_at(mSmallBinnedMallocators[i]);
			mOSPageMallocator.FreeSmall(mSmallBinnedMallocators[i]);
		}
	}
	/**
	 * Free all heaps
	 */
	while (mHeapChainHead)
	{
		auto Next = mHeapChainHead->Next;
		std::destroy_at(mHeapChainHead);
		mOSPageMallocator.FreeSmall(mHeapChainHead);
		mHeapChainHead = Next;
	}
}

bool FVMArena::Owned(void* p)
{
	return mOSPageMallocator.Owned(p);
}

void* FVMArena::Malloc(size_t size)
{
	return size <= HLVM_VMA_SMALL_ALLOC_THRESHOLD ? MallocSmallBinned(size) : MallocHeap(size);
}

void FVMArena::Free(void* p)
{
	if (mPendingFressLists.GenericFreeList.size() == mPendingFressLists.GenericFreeList.max_size())
	{
		while (mPendingFressLists.GenericFreeList.size())
		{
			auto LastGenericPtr = mPendingFressLists.GenericFreeList.back();
			if (Owned(LastGenericPtr))
			{
				if (mPendingFressLists.LocalFreeList.size() == mPendingFressLists.LocalFreeList.max_size())
				{
					while (mPendingFressLists.LocalFreeList.size())
					{
						/**
						 * Free local free list, a local pointer is either from bin or heap
						 */
						auto LastLocalPtr = mPendingFressLists.LocalFreeList.back();
						if (auto alignment = FSmallBinnedBlockHead::IsSmallAlloc(LastLocalPtr);
							alignment != 0)
						{
							FreeSmallBinned(LastLocalPtr, alignment);
						}
						else
						{
							FreeHeap(LastLocalPtr);
						}
						mPendingFressLists.LocalFreeList.pop_back();
					}
				}
				mPendingFressLists.LocalFreeList.push_back(LastGenericPtr);
			}
			else
			{
				if (mPendingFressLists.NonLocalFreeList.size() == mPendingFressLists.NonLocalFreeList.max_size())
				{
					while (mPendingFressLists.NonLocalFreeList.size())
					{
						auto LastNonLocalPtr = mPendingFressLists.NonLocalFreeList.back();
						sGlobalPendingFreeList.Push({ .ptrToBeFree = LastNonLocalPtr, .tidNotOwned = GCurrentTID64 });
						mPendingFressLists.NonLocalFreeList.pop_back();
					}
				}
				mPendingFressLists.NonLocalFreeList.push_back(LastGenericPtr);
			}
			mPendingFressLists.GenericFreeList.pop_back();
		}
	}
	mPendingFressLists.GenericFreeList.push_back(p);

	/**
	 * Try to pop from local pending free list to pending local free list
	 */
	while (mPendingFressLists.LocalFreeList.size() < mPendingFressLists.LocalFreeList.max_size())
	{
		FLocalPendingFree PendingFree;
		if (mThisThreadPendingFreeList.PopFront(PendingFree))
		{
			mPendingFressLists.LocalFreeList.push_back(PendingFree.ptrToBeFree);
		}
		else
		{
			break;
		}
	}
}

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
	Heap->Next = std::construct_at(R_C(FVMHeapChain*, mOSPageMallocator.MallocSmall(sizeof(FVMHeapChain))));
	auto& HeapMallocator = Heap->Next->HeapAllocator;
	/**
	 * Initialize heap with the size of the allocation
	 */
	if (FVMHeap::EstimateHeapCapacityBySize(size) <= HLVM_VMA_LARGE_HEAP_SIZE)
	{
		HeapMallocator.Init(this, HLVM_VMA_LARGE_HEAP_SIZE);
	}
	else
	{
		// Init heap with unmanged setting if the size is too large
		HeapMallocator.Init(this, size, true);
	}
	HLVM_CONSTEXPR_ASSERT(bValidate, HeapMallocator.GetHeapSize() >= size);
	auto p = HeapMallocator.Malloc(size);
	HLVM_CONSTEXPR_ASSERT(bValidate, p != nullptr);
	return p;
}

void FVMArena::FreeHeap(void* p)
{
	// Use 32MB mask to find head pointer of 32MB page which holds some pointers that point to allocator
#if 1
	auto	 HeapHeadBlock = R_C(FVMHeap::FHeapHeadBlock*, AlignDown(p, HLVM_VMA_LARGE_HEAP_SIZE));
	FVMHeap* OwnerHeap = HeapHeadBlock->OwnerHeap;
	if constexpr (bValidate)
	{
		auto Heap = mHeapChainHead;
		while (Heap)
		{
			auto& HeapMallocator = Heap->HeapAllocator;
			if (&HeapMallocator == OwnerHeap)
			{
				HLVM_ENSURE(HeapMallocator.Owned(p), TXT("FVMArena::FreeHeap : Heap {} not own pointer {}"),
					R_C(void*, OwnerHeap), R_C(void*, p));
				HeapMallocator.Free(p);
				return;
			}
			Heap = Heap->Next;
		}
	}
	else
	{
		auto& HeapMallocator = *OwnerHeap;
		HLVM_ENSURE(HeapMallocator.Owned(p), TXT("FVMArena::FreeHeap : Heap {} not own pointer {}"),
			R_C(void*, OwnerHeap), R_C(void*, p));
		HeapMallocator.Free(p);
		return;
	}
#else // No use 32MB heap alignment trick, just traverse all heaps
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
#endif
	HLVM_ENSURE(false, TXT("FVMArena::FreeHeap : Failed to free {} from heap"), R_C(void*, p));
}

void* FVMArena::MallocSmallBinned(size_t _size)
{
	HLVM_CONSTEXPR_ASSERT(bValidate, _size <= HLVM_VMA_SMALL_ALLOC_THRESHOLD);
	TUINT8 size = FSmallBinnedBlockHead::GoodSize(_size);
	auto   p = mSmallBinnedMallocators[size / HLVM_VMA_SMALL_ALLOC_ALIGNMENT - 1]->Malloc();
	HLVM_CONSTEXPR_ASSERT(bValidate, p != nullptr);
	return p;
}

void FVMArena::FreeSmallBinned(void* p, TUINT8 size)
{
	HLVM_CONSTEXPR_ASSERT(bValidate, size <= HLVM_VMA_SMALL_ALLOC_THRESHOLD);
	mSmallBinnedMallocators[size / HLVM_VMA_SMALL_ALLOC_ALIGNMENT - 1]->Free(p);
}

void* FVMArena::MallocOSPage(size_t N)
{
	const bool bValid = N <= HLVM_VMA_LARGE_HEAP_SIZE;
	HLVM_CONSTEXPR_ASSERT(bValidate, bValid);
	return mOSPageMallocator.MallocLargeHeap();
}

void FVMArena::FreeOSPage(void* p)
{
	mOSPageMallocator.FreeLargeHeap(p);
}

void* FVMArena::MallocLowLevel(size_t size)
{
	return mOSPageMallocator.MallocAlign(size);
}

void FVMArena::FreeLowLevel(void* p)
{
	HLVM_ENSURE(mOSPageMallocator.FreeAlign(p) == EFreeRetType::Success,
		TXT("FreeLowLevel failed {}"), R_C(void*, p));
}
