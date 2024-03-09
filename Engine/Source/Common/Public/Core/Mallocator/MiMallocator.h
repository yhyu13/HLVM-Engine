/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "IMallocator.h"

// https://github.com/microsoft/mimalloc
#include <mimalloc.h>

struct FMiMallocatorContext
{
	bool bNewHeap{ false };
	BIT_FLAG(bDestory){ false }; // CAUTION: Free all allocated heap w/o checking if pages still persist
};

class FMiMallocator final : public IMallocator
{
public:
	NOCOPYMOVE(FMiMallocator)
	FMiMallocator(const FMiMallocatorContext& _Ctx = FMiMallocatorContext())
		: mCtx(_Ctx)
	{
		Type = EMallocator::Mimalloc;
		mHeap = (mCtx.bNewHeap ? mi_heap_new() : mi_heap_get_default());
	}
	~FMiMallocator() final override
	{
		if (mCtx.bNewHeap)
		{
			if (mCtx.bDestory)
				HLVM_UNLIKELY
				{
					mi_heap_destroy(mHeap);
				}
			else
			{
				mi_heap_delete(mHeap);
			}
		}
		else
		{
			mi_heap_collect(mHeap, false);
		}
	}
	virtual bool				   Owened(void* ptr) noexcept final override;
	HLVM_INLINE_FUNC virtual void* Malloc(size_t size) noexcept(false) final override
	{
		return mi_heap_malloc(mHeap, size);
	}
	HLVM_INLINE_FUNC virtual void* Malloc2(size_t size) noexcept final override
	{
		return mi_heap_malloc(mHeap, size);
	}
	HLVM_INLINE_FUNC virtual void* MallocAligned(size_t size, size_t alignment) noexcept(false) final override
	{
		return mi_heap_malloc_aligned(mHeap, size, alignment);
	}
	HLVM_INLINE_FUNC virtual void* MallocAligned2(size_t size, size_t alignment) noexcept final override
	{
		return mi_heap_malloc_aligned(mHeap, size, alignment);
	}
	HLVM_INLINE_FUNC virtual void Free(void* ptr) noexcept final override
	{
		mi_free(ptr);
	}
	HLVM_INLINE_FUNC virtual void FreeSize(void* ptr, size_t size) noexcept final override
	{
		mi_free_size(ptr, size);
	}
	HLVM_INLINE_FUNC virtual void FreeAligned(void* ptr, size_t alignment) noexcept final override
	{
		mi_free_aligned(ptr, alignment);
	}
	HLVM_INLINE_FUNC virtual void FreeSizeAligned(void* ptr, size_t size, size_t alignment) noexcept final override
	{
		mi_free_size_aligned(ptr, size, alignment);
	}

private:
	mi_heap_t*			 mHeap;
	FMiMallocatorContext mCtx;
};
HLVM_TLS_VAR HLVM_INLINE_VAR FMiMallocator GMiMallocatorTLS{};
