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
	auto _Heap = mHeapChainHead; // Create a dummy that holds the head of the heap chain temporarily
	for (size_t i = 0; i < mInitCtx.LargeHeapInitNum; ++i)
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
		HLVM_LOG(LogVMArena, debug, TXT("VMArena: Initialized %d large heaps"), i + 1);
	}

	/**
	 * Intialize small binning allocators (using fancy compile time for-loop)
	 */
	ct_for<0, HLVM_VMA_SMALL_ALLOC_THRESHOLD / HLVM_VMA_SMALL_ALLOC_ALIGNMENT, 1, TUINT8>([&](auto i) {
		// Initialize binned allocator
		using BinnedMallocatorType = FSmallBinnedMallocator<(i.value + 1) * HLVM_VMA_SMALL_ALLOC_ALIGNMENT>;
		mSmallBinnedMallocators[i] = std::construct_at(R_C(BinnedMallocatorType*, mOSPageMallocator.MallocSmall(sizeof(BinnedMallocatorType))));
		mSmallBinnedMallocators[i]->Init(this);
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
		if (mOSPageMallocator.Owned(LastGenericPtr))
		{
			/**
			 * Ignore freeing of memory owned by current VM that is about to be destructed
			 */
			continue;
		}
		else
		{
			while (mPendingFressLists.NonLocalFreeList.size())
			{
				// auto LastNonLocalPtr = mPendingFressLists.NonLocalFreeList.back();
				//  TODO : Push LastNonLocalPtr to global free list
				mPendingFressLists.NonLocalFreeList.pop_back();
			}
		}
		mPendingFressLists.GenericFreeList.pop_back();
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

void* FVMArena::Malloc(size_t size)
{
	return size <= HLVM_VMA_SMALL_ALLOC_THRESHOLD ? MallocBinned(size) : MallocHeap(size);
}

void FVMArena::Free(void* p)
{
	if (mPendingFressLists.GenericFreeList.size() == mPendingFressLists.GenericFreeList.max_size())
	{
		while (mPendingFressLists.GenericFreeList.size())
		{
			auto LastGenericPtr = mPendingFressLists.GenericFreeList.back();
			if (mOSPageMallocator.Owned(LastGenericPtr))
			{
				if (mPendingFressLists.LocalFreeList.size() == mPendingFressLists.LocalFreeList.max_size())
				{
					while (mPendingFressLists.LocalFreeList.size())
					{
						auto LastLocalPtr = mPendingFressLists.LocalFreeList.back();
						if (auto alignment = FSmallBinnedBlockHead::IsSmallAlloc(LastLocalPtr))
						{
							FreeBinned(LastLocalPtr, alignment);
						}
						else
						{
							FreeHeap(LastLocalPtr);
						}
						mPendingFressLists.LocalFreeList.pop_back();
					}
				}
				else
				{
					mPendingFressLists.LocalFreeList.push_back(LastGenericPtr);
				}
			}
			else
			{
				if (mPendingFressLists.NonLocalFreeList.size() == mPendingFressLists.NonLocalFreeList.max_size())
				{
					while (mPendingFressLists.NonLocalFreeList.size())
					{
						// auto LastNonLocalPtr = mPendingFressLists.NonLocalFreeList.back();
						//  TODO : Push LastNonLocalPtr to global free list
						mPendingFressLists.NonLocalFreeList.pop_back();
					}
				}
				else
				{
					mPendingFressLists.NonLocalFreeList.push_back(LastGenericPtr);
				}
			}
			mPendingFressLists.GenericFreeList.pop_back();
		}
	}
	else
	{
		mPendingFressLists.GenericFreeList.push_back(p);
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
	// TODO : use 32MB alignment to quickly find heap allocation instead of traversal
	// Use 32MB mask to find head pointer of 32MB page which holds some pointers that point to allocator
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
	HLVM_ENSURE(false, TXT("FVMArena::FreeHeap : Failed to free heap allocation"));
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
	return GStdMallocator.Malloc(size);
}

void FVMArena::FreeLowLevel(void* p)
{
	HLVM_ENSURE(GStdMallocator.Free(p) == EFreeRetType::Success,
		TXT("FreeLowLevel failed {}"), R_C(void*, p));
}
