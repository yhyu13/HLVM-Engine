/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Mallocator/PMR.h"
#include "Core/Parallel/Lock.h"
#include "Core/Container/ContainerDefinition.h"
#include "ISmallBinnedMallocator.h"
#include "VMHeap.h"
#include "OSPageMallocator.h"

struct FVMArenaInitContext
{
	size_t LargeHeapSize{ HLVM_VMA_LARGE_HEAP_SIZE };
	size_t LargeHeapInitNum{ 0 };

	bool Valid() const
	{
		return LargeHeapSize > 0
			&& LargeHeapSize <= HLVM_VMA_LARGE_HEAP_SIZE;
	}
};

/**
 * Malloc reserved memory pages as virtual memory arena, and return a sized block within a page on malloc.
 * When out of memory, trigger a page fault, malloc a new reserved memory block, and continue on.
 */
class FVMArena
{
	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr bool bValidate = HLVM_MALLOC_VALIDATION;

public:
	NOCOPYMOVE(FVMArena)
	FVMArena(const FVMArenaInitContext& _InitContext = FVMArenaInitContext{});
	~FVMArena();

	void* Malloc(size_t size);
	void  Free(void* p);

	void* MallocHeap(size_t size);
	void  FreeHeap(void* p);

	void* MallocBinned(size_t size);
	void  FreeBinned(void* p, TUINT8 size);

	void* MallocOSPage(size_t size, size_t alignment);
	void  FreeOSPage(void* p, size_t alignment);

	void* MallocLowLevel(size_t size);
	void  FreeLowLevel(void* p);

private:
	struct FVMArenaShared
	{
		// Use RW lock to protect shared data
		FRWRivalLock RWLock{};

		// Shared lookup table for Threaded free list (which all uses std allocator underneath)
		using Tid = std::thread::id;
		using TLSList = TVector<void*, TStdMallocator<void*>>*;
		TStableMap<Tid, TLSList, TStdMallocator<std::pair<Tid, TLSList>>> ThreadFreeListMap;
	};

private:
	friend class ISmallBinnedMallocator;

	struct FVMHeapChain
	{
		FVMHeap		  HeapAllocator{};
		FVMHeapChain* Next{ nullptr };
	};
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

	FPendingFreeLists		mPendingFressLists;
	ISmallBinnedMallocator* mSmallBinnedMallocators[HLVM_VMA_SMALL_ALLOC_THRESHOLD / HLVM_VMA_SMALL_ALLOC_ALIGNMENT];
	FVMHeapChain*			mHeapChainHead{ nullptr };
	FOSPageMallocator		mOSPageMallocator{};
	FVMArenaInitContext		mInitCtx{};
};
