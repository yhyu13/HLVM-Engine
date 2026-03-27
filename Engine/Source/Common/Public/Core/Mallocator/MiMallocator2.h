/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "IMallocator.h"
#include "Core/Mallocator/Mi/MiMalloc.h"

struct FMiMallocator2Context
{
	bool bNewHeap{ false };
	BIT_FLAG(bDestory){ false }; // CAUTION: Free all allocated heap w/o checking if pages still persist
};

class FMiMallocator2 final : public IMallocator
{
public:
	NOCOPYMOVE(FMiMallocator2);
	FMiMallocator2(const FMiMallocator2Context& _Ctx = FMiMallocator2Context()) noexcept
		: mCtx(_Ctx)
	{
		Type = EMallocator::MiMalloc2;
	}
	virtual ~FMiMallocator2() noexcept final override
	{
		if (mCtx.bNewHeap)
			HLVM_UNLIKELY
		{
			if (mCtx.bDestory)
				HLVM_UNLIKELY
			{
				// MiMalloc2 doesn't support heap destruction - collect instead
				mi::Mallocator::instance().thread_done();
			}
			else
				HLVM_LIKELY
			{
				// Thread cleanup
				mi::Mallocator::instance().thread_done();
			}
		}
		else
			HLVM_LIKELY
		{
			// Just collect
			mi::Mallocator::instance().thread_done();
		}
	}
	HLVM_NODISCARD HLVM_NOINLINE_FUNC virtual bool Owned(void* ptr) noexcept final override
	{
		return mi::Mallocator::instance().pointer_is_valid(ptr);
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual void* Malloc(size_t size) noexcept(false) final override
	{
		return mi::Mallocator::instance().allocate(size);
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual void* Malloc2(size_t size) noexcept final override
	{
		return mi::Mallocator::instance().allocate(size);
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual void* MallocAligned(size_t size, size_t alignment) noexcept(false) final override
	{
		return mi::Mallocator::instance().allocate_aligned(size, alignment);
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual void* MallocAligned2(size_t size, size_t alignment) noexcept final override
	{
		return mi::Mallocator::instance().allocate_aligned(size, alignment);
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual EFreeRetType Free(void* ptr) noexcept final override
	{
		mi::Mallocator::instance().free(ptr);
		return EFreeRetType::Success;
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual EFreeRetType FreeSize(void* ptr, size_t size) noexcept final override
	{
		// MiMalloc2 doesn't have free_size, just use free
		(void)size; // Unused
		mi::Mallocator::instance().free(ptr);
		return EFreeRetType::Success;
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual EFreeRetType FreeAligned(void* ptr, size_t alignment) noexcept final override
	{
		// MiMalloc2 uses single free function for both aligned and non-aligned
		(void)alignment; // Unused
		mi::Mallocator::instance().free(ptr);
		return EFreeRetType::Success;
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual EFreeRetType FreeSizeAligned(void* ptr, size_t size, size_t alignment) noexcept final override
	{
		(void)size; // Unused
		(void)alignment; // Unused
		mi::Mallocator::instance().free(ptr);
		return EFreeRetType::Success;
	}

	HLVM_INLINE_FUNC virtual void Collect(bool bForce) noexcept final override
	{
		(void)bForce; // Unused - MiMalloc2 handles collection internally via thread_done
		mi::Mallocator::instance().thread_done();
	}

private:
	FMiMallocator2Context mCtx;
};

#if HLVM_MALLOC_USE_MIMALLOC2_OVER_STD
HLVM_THREAD_LOCAL_VAR HLVM_INLINE_VAR FMiMallocator2 GMiMallocator2TLS{};
	#undef HLVM_LOW_GMALLOC_TLS
	#define HLVM_LOW_GMALLOC_TLS GMiMallocator2TLS
#endif
