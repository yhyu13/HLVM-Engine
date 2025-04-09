/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Mallocator/PMR.h"
#include "Core/Container/ContainerDefinition.h"
#include "Core/Parallel/ConcurrentQueue.h"

#include "ISmallBinnedMallocator.h"
#include "VMHeap.h"
#include "OSPageMallocator.h"
#include "Core/Parallel/Lock.h"

/**
 * Malloc reserved memory pages as virtual memory arena, and return a sized block within a page on malloc.
 * When out of memory, trigger a page fault, malloc a new reserved memory block, and continue on.
 */
class FVMArena : public FAtomicFlagS<FVMArena>
{
	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr bool bValidate = HLVM_MALLOC_VALIDATION;

public:
	NOCOPYMOVE(FVMArena)
	FVMArena();
	~FVMArena();

	bool Owned(void* p);

	void* Malloc(size_t size);
	void  Free(void* p);

	void* MallocHeap(size_t size);
	void  FreeHeap(void* p);

	void* MallocSmallBinned(size_t size);
	void  FreeSmallBinned(void* p, TUINT8 size);

private:
	friend FVMHeap;

	void* MallocOSPage(size_t size);
	void  FreeOSPage(void* p);

	void* MallocLowLevel(size_t size);
	void  FreeLowLevel(void* p);

	static void NonLocalFreeHandler();

private:
	struct FNonLocalPendingFree
	{
		void*	  ptrToBeFree;
		FVMArena* ArenaNotOwned; // Helper data which we already known a arena that ptr does not belong to
	};
	/**
	 * Global pending free list, accepting pending free pointer from non-local frees,
	 * and then pump these pending free to corresponding threads's local free list.
	 */
	HLVM_STATIC_VAR TConcurrentQueue<FNonLocalPendingFree,
		EConcurrentQueueMode::Mpsc, false,
		TPMRLowLvl<TConcurrentQueueNode<FNonLocalPendingFree>>>
					sNonLocalPendingFreeList;
	HLVM_STATIC_VAR TSmallVector64<FVMArena*, TPMRLowLvl<FVMArena*>> sGlobalArenaList;

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
		TPMRLowLvl<TConcurrentQueueNode<FLocalPendingFree>>>
		mLocalPendingFreeList;

	// Pending free list
	struct FPendingFreeLists
	{
		// pending free pointer that needs to be determined whether or not owned by this arena
		TStaticVector<void*, HLVM_VMA_GENERIC_PENDING_FREE_LIST_SIZE> GenericFreeList;
		// pending free pointer owned by ths arena
		TStaticVector<void*, HLVM_VMA_LOCAL_PENDING_FREE_LIST_SIZE> LocalFreeList;
		// pending free pointer not owned by ths arena
		TStaticVector<void*, HLVM_VMA_NONLOCAL_PENDING_FREE_LIST_SIZE> NonLocalFreeList;
	};
	static_assert(sizeof(FPendingFreeLists) >= sizeof(void*) * HLVM_VMA_GENERIC_PENDING_FREE_LIST_SIZE
				+ sizeof(void*) * HLVM_VMA_LOCAL_PENDING_FREE_LIST_SIZE
				+ sizeof(void*) * HLVM_VMA_NONLOCAL_PENDING_FREE_LIST_SIZE,
		"Pending free list size is too small, will trigger static vector's heap allocation instead of stack allocation");
	FPendingFreeLists mPendingFreeLists{};

	// Small binned allocator
	ISmallBinnedMallocator* mSmallBinnedMallocators[hlvm_vma_small_binned_alloc_num];
	// OS page allocator
	FOSPageMallocator mOSPageMallocator{};

	struct FVMHeapChain
	{
		FVMHeap		  HeapAllocator{};
		FVMHeapChain* Next{ nullptr };
	};
	// VM heap
	FVMHeapChain* mHeapChainHead{ nullptr };
};
