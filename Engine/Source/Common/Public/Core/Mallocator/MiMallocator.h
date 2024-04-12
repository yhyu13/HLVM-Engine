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
	FMiMallocator(const FMiMallocatorContext& _Ctx = FMiMallocatorContext()) noexcept
		: mCtx(_Ctx)
	{
		Type = EMallocator::Mimalloc;
		mHeap = (mCtx.bNewHeap ? mi_heap_new() : mi_heap_get_default());
	}
	virtual ~FMiMallocator() noexcept final override
	{
		if (mCtx.bNewHeap)
			HLVM_UNLIKELY
			{
				if (mCtx.bDestory)
					HLVM_UNLIKELY
					{
						mi_heap_destroy(mHeap);
					}
				else
					HLVM_LIKELY
					{
						mi_heap_delete(mHeap);
					}
			}
		else
			HLVM_LIKELY
			{
				mi_heap_collect(mHeap, false);
			}
	}
	HLVM_NODISCARD HLVM_NOINLINE_FUNC virtual bool Owned(void* ptr) noexcept final override;
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual void*  Malloc(std::size_t size) noexcept(false) final override
	{
		return mi_heap_malloc(mHeap, size);
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual void* Malloc2(std::size_t size) noexcept final override
	{
		return mi_heap_malloc(mHeap, size);
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual void* MallocAligned(std::size_t size, std::size_t alignment) noexcept(false) final override
	{
		return mi_heap_malloc_aligned(mHeap, size, alignment);
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual void* MallocAligned2(std::size_t size, std::size_t alignment) noexcept final override
	{
		return mi_heap_malloc_aligned(mHeap, size, alignment);
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual EFreeRetType Free(void* ptr) noexcept final override
	{
		mi_free(ptr);
		return EFreeRetType::Success;
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual EFreeRetType FreeSize(void* ptr, std::size_t size) noexcept final override
	{
		mi_free_size(ptr, size);
		return EFreeRetType::Success;
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual EFreeRetType FreeAligned(void* ptr, std::size_t alignment) noexcept final override
	{
		mi_free_aligned(ptr, alignment);
		return EFreeRetType::Success;
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual EFreeRetType FreeSizeAligned(void* ptr, std::size_t size, std::size_t alignment) noexcept final override
	{
		mi_free_size_aligned(ptr, size, alignment);
		return EFreeRetType::Success;
	}

private:
	mi_heap_t*			 mHeap;
	FMiMallocatorContext mCtx;
};
HLVM_TLS_VAR HLVM_INLINE_VAR FMiMallocator GMiMallocatorTLS{};
