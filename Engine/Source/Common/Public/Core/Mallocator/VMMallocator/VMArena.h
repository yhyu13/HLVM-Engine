/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Mallocator/PMR.h"
#include "Core/Container/ContainerDefinition.h"
#include "Core/Parallel/ConcurrentQueue.h"

#include "ISmallBinnedMallocator.h"
#include "VMHeap.h"
#include "OSPageMallocator.h"

/**
 * Malloc reserved memory pages as virtual memory arena, and return a sized block within a page on malloc.
 * When out of memory, trigger a page fault, malloc a new reserved memory block, and continue on.
 */
class FVMArena
{
	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr bool bValidate = HLVM_MALLOC_VALIDATION;

public:
	NOCOPYMOVE(FVMArena)
	FVMArena();
	~FVMArena();

	void* Malloc(size_t size);
	void  Free(void* p);

	void* MallocHeap(size_t size);
	void  FreeHeap(void* p);

	void* MallocBinned(size_t size);
	void  FreeBinned(void* p, TUINT8 size);

	void* MallocOSPage(size_t size);
	void  FreeOSPage(void* p);

	void* MallocLowLevel(size_t size);
	void  FreeLowLevel(void* p);

private:
	struct FNonLocalPendingFree
	{
		void*			ptrToBeFree;
		std::thread::id tidNotOwned; // Helper data which we already known a tid that ptr does not belong to
	};
	/**
	 * Global pending free list, accepting pending free pointer from non-local frees,
	 * and then pump these pending free to corresponding local free list.
	 */
	HLVM_INLINE_VAR HLVM_STATIC_VAR TConcurrentQueue<FNonLocalPendingFree,
		EConcurrentQueueMode::Mpsc, false,
		TPMRGMallocator<hlvm_private::TQueueNode<FNonLocalPendingFree>>>
									sGlobalPendingFreeList;

private:
	struct FPendingFreeLists
	{
		TFixedSizeVector<void*, HLVM_VMA_GENERIC_PENDING_FREE_LIST_SIZE>  GenericFreeList;
		TFixedSizeVector<void*, HLVM_VMA_LOCAL_PENDING_FREE_LIST_SIZE>	  LocalFreeList;
		TFixedSizeVector<void*, HLVM_VMA_NONLOCAL_PENDING_FREE_LIST_SIZE> NonLocalFreeList;
	};
	static_assert(sizeof(FPendingFreeLists) >= sizeof(void*) * HLVM_VMA_GENERIC_PENDING_FREE_LIST_SIZE
				+ sizeof(void*) * HLVM_VMA_LOCAL_PENDING_FREE_LIST_SIZE
				+ sizeof(void*) * HLVM_VMA_NONLOCAL_PENDING_FREE_LIST_SIZE,
		"Pending free list size is too small, potential heap allocation instead of stack allocation");

	struct FVMHeapChain
	{
		FVMHeap		  HeapAllocator{};
		FVMHeapChain* Next{ nullptr };
	};

	FPendingFreeLists		mPendingFressLists{};
	ISmallBinnedMallocator* mSmallBinnedMallocators[HLVM_VMA_SMALL_ALLOC_THRESHOLD / HLVM_VMA_SMALL_ALLOC_ALIGNMENT];
	FVMHeapChain*			mHeapChainHead{ nullptr };
	FOSPageMallocator		mOSPageMallocator{};

	/**
	 * Local pending free list pumped by global free list
	 * These free pointers come from global free list finding a corresponding local free list
	 */
	struct FLocalPendingFree
	{
		void* ptrToBeFree;
	};
	TConcurrentQueue<FLocalPendingFree,
		EConcurrentQueueMode::Spsc, false,
		TPMRGMallocator<hlvm_private::TQueueNode<FLocalPendingFree>>>
		mLocalPendingFreeListReceiver;
};
