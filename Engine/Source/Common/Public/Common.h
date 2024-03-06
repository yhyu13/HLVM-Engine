/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once
#include "GlobalDefinition.h"
#include "Platform/PlatformDefinition.h"
#include "Core/Container/ContainerDefinition.h"
#include "Core/Parallel/ParallelDefinition.h"
#include "Template/GlobalTemplate.tpp"

// https://github.com/microsoft/mimalloc
#include <mimalloc.h>

HLVM_ENUM(EMallocator, uint8_t,
	Mimalloc,
	Stack,
	Unkown);

/**
 * Mallocator interface class
 * Default allocator is Mimalloc
 */
class IMallocator
{
public:
	NOCOPYMOVE(IMallocator)
	IMallocator() = default;
	virtual ~IMallocator() = default;
	HLVM_INLINE_FUNC virtual bool  Owened(void* ptr) noexcept = 0;
	HLVM_INLINE_FUNC virtual void* Malloc(size_t size) noexcept(false) = 0;
	HLVM_INLINE_FUNC virtual void* Malloc2(size_t size) noexcept = 0;
	HLVM_INLINE_FUNC virtual void* MallocAligned(size_t size, size_t alignment) noexcept(false) = 0;
	HLVM_INLINE_FUNC virtual void* MallocAligned2(size_t size, size_t alignment) noexcept = 0;
	HLVM_INLINE_FUNC virtual void  Free(void* ptr) noexcept = 0;
	HLVM_INLINE_FUNC virtual void  FreeSize(void* ptr, size_t size) noexcept = 0;
	HLVM_INLINE_FUNC virtual void  FreeAligned(void* ptr, size_t alignment) noexcept = 0;
	HLVM_INLINE_FUNC virtual void  FreeSizeAligned(void* ptr, size_t size, size_t alignment) noexcept = 0;

	EMallocator Type = EMallocator::Unkown;
};
/**
 * Global mallocator
 */
void									  InitMallocator();
HLVM_TLS_VAR HLVM_EXTERN_VAR IMallocator* GMallocatorTLS;
namespace hlvm_private
{
	HLVM_TLS_VAR HLVM_INLINE_VAR IMallocator* GMallocatorTLSSwap = nullptr;
}
HLVM_INLINE_FUNC void SwapMallocator(IMallocator* Mallocator = nullptr)
{
	if (hlvm_private::GMallocatorTLSSwap == nullptr)
	{
		hlvm_private::GMallocatorTLSSwap = GMallocatorTLS;
		GMallocatorTLS = Mallocator;
	}
	else
	{
		GMallocatorTLS = hlvm_private::GMallocatorTLSSwap;
		hlvm_private::GMallocatorTLSSwap = nullptr;
	}
}

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
	~FMiMallocator() override
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

// TODO Stack allocator
