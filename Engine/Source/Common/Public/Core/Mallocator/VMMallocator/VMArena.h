/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Mallocator/MallocatorDefinition.h"
#include "ISmallBinnedMallocator.h"
#include "HeapMallocator.h"
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
	struct FHeapChain
	{
		FHeapMallocator HeapAllocator{};
		FHeapChain*		Next{ nullptr };
	};

	friend class ISmallBinnedMallocator;

	ISmallBinnedMallocator* mSmallBinnedMallocators[HLVM_VMA_SMALL_ALLOC_THRESHOLD / HLVM_VMA_SMALL_ALLOC_ALIGNMENT];
	FHeapChain*				mHeapChainHead{ nullptr };
	FOSPageMallocator		mOSPageMallocator{};
	FVMArenaInitContext		mInitCtx{};
};
