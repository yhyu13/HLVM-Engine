/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Mallocator/MallocatorDefinition.h"
#include "ISmallBinnedMallocator.h"
#include "HeapMallocator.h"

#ifndef HLVM_VMA_DEFAULT_HEAP_SIZE
	#define HLVM_VMA_DEFAULT_HEAP_SIZE 1 * 1024 * 1024
#endif

#ifndef HLVM_VMA_LARGE_HEAP_SIZE_FACTOR
	#define HLVM_VMA_LARGE_HEAP_SIZE_FACTOR 4
#endif

/**
 * Malloc reserved memory pages as virtual memory arena, and return a sized block within a page on malloc.
 * When out of memory, trigger a page fault, malloc a new reserved memory block, and continue on.
 */
class FVMArena
{
	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr bool bValidate = HLVM_MALLOC_VALIDATION;

public:
	NOCOPYMOVE(FVMArena)
	FVMArena(FMiMallocator* _MiMallocator = &GMiMallocatorTLS,
		size_t				_DefaultHeapSize = HLVM_VMA_DEFAULT_HEAP_SIZE,
		size_t				_LargeHeapSize = HLVM_VMA_DEFAULT_HEAP_SIZE * HLVM_VMA_LARGE_HEAP_SIZE_FACTOR);
	~FVMArena();

	void* MallocHeap(size_t size);
	void* MallocBinned(size_t size);
	void  FreeHeap(void* p);
	void  FreeBinned(void* p, TUINT8 size);

private:
	struct FHeapChain
	{
		FHeapMallocator HeapAllocator{};
		FHeapChain*		Next{ nullptr };
	};

	ISmallBinnedMallocator* mSmallBinnedMallocators[HLVM_SMALL_ALLOC_THRESHOLD / HLVM_SMALL_ALLOC_ALIGNMENT];
	FMiMallocator*			MiMallocator{ nullptr };
	FHeapChain*				mHeapChainHead{ nullptr };
	size_t					mDefaultHeapSize{ HLVM_VMA_DEFAULT_HEAP_SIZE };
	size_t					mLargeHeapSize{ HLVM_VMA_DEFAULT_HEAP_SIZE * HLVM_VMA_LARGE_HEAP_SIZE_FACTOR };
};
