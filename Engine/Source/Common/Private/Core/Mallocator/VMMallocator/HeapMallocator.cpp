/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Mallocator/VMMallocator/HeapMallocator.h"

void* FHeapMallocator::Malloc(size_t size)
{
	(void)size;
	if (!bManaged)
	{
		HLVM_CONSTEXPR_ASSERT(bValidate, mHeap != nullptr);
		return mHeap;
	}
	else
	{
		// TODO
		return nullptr;
	}
}

void FHeapMallocator::Free(void* p)
{
	(void)p;
	if (!bManaged)
	{
		Destroy();
		return;
	}
	HLVM_CONSTEXPR_ASSERT(bValidate, Owned(p));
	// TODO
	{
	}
}
