/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Mallocator/MallocatorDefinition.h"

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
public:
	using SizeType = int32_t;

	NOCOPYMOVE(FVMArena);
	FVMArena() = default;
	FVMArena(SizeType DefaultHeapSize = HLVM_VMA_DEFAULT_HEAP_SIZE,
		SizeType	  LargeHeapSize = HLVM_VMA_DEFAULT_HEAP_SIZE * HLVM_VMA_LARGE_HEAP_SIZE_FACTOR);
	~FVMArena();

	void* malloc(SizeType size);
	void  free(void* ptr);

private:
	struct FHeapBlock
	{
		void*		mHeap{ nullptr };
		SizeType	mSize{ 0 };
		FHeapBlock* pNextBlock{ nullptr };
	};

	FHeapBlock mHeapBlockHead{};
	SizeType   mDefaultHeapSize{ HLVM_VMA_DEFAULT_HEAP_SIZE };
	SizeType   mLargeHeapSize{ HLVM_VMA_DEFAULT_HEAP_SIZE * HLVM_VMA_LARGE_HEAP_SIZE_FACTOR };
};
