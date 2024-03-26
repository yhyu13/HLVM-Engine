/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Mallocator/MallocatorDefinition.h"
#include "ISmallBinnedMallocator.h"
#include "HeapMallocator.h"

#ifndef HLVM_VMA_DEFAULT_HEAP_SIZE
	#define HLVM_VMA_DEFAULT_HEAP_SIZE 4 * 1024 * 1024 // Must be power of 2
#endif
static_assert((HLVM_VMA_DEFAULT_HEAP_SIZE & (HLVM_VMA_DEFAULT_HEAP_SIZE - 1)) == 0, "Default heap size must be power of 2");

#ifndef HLVM_VMA_LARGE_HEAP_SIZE
	#define HLVM_VMA_LARGE_HEAP_SIZE 4 * HLVM_VMA_DEFAULT_HEAP_SIZE // Must be power of 2
#endif
static_assert((HLVM_VMA_LARGE_HEAP_SIZE & (HLVM_VMA_LARGE_HEAP_SIZE - 1)) == 0, "Default heap size must be power of 2");

struct FVMArenaInitContext
{
	size_t DefaultHeapSize{ HLVM_VMA_DEFAULT_HEAP_SIZE };
	size_t DefaultHeapInitNum{ 1 };
	size_t LargeHeapSize{ HLVM_VMA_LARGE_HEAP_SIZE };
	size_t LargeHeapInitNum{ 0 };

	bool Valid() const
	{
		return DefaultHeapSize > 0
			&& DefaultHeapInitNum >= 1
			&& LargeHeapSize > 0
			&& DefaultHeapSize % HLVM_MALLOC_ALIGNMENT == 0
			&& LargeHeapSize % HLVM_MALLOC_ALIGNMENT == 0;
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
	FVMArena(FMiMallocator* _MiMallocator = &GMiMallocatorTLS, const FVMArenaInitContext& _InitContext = FVMArenaInitContext{});
	~FVMArena();

	void* MallocHeap(size_t size);
	void* MallocBinned(size_t size);
	void  FreeHeap(void* p);
	void  FreeBinned(void* p, TUINT8 size);

	void* MallocOS(size_t size, size_t alignment = HLVM_MALLOC_ALIGNMENT);
	void  FreeOS(void* p);

private:
	struct FHeapChain
	{
		FHeapMallocator HeapAllocator{};
		FHeapChain*		Next{ nullptr };
	};

	ISmallBinnedMallocator* mSmallBinnedMallocators[HLVM_SMALL_ALLOC_THRESHOLD / HLVM_SMALL_ALLOC_ALIGNMENT];
	FMiMallocator*			MiMallocator{ nullptr };
	FHeapChain*				mHeapChainHead{ nullptr };
	FVMArenaInitContext		mInitCtx{};
};
