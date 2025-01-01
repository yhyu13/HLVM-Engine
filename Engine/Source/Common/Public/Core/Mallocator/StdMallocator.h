/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "IMallocator.h"
#include <stdlib.h>

class FStdMallocator final : public IMallocator
{
public:
	NOCOPYMOVE(FStdMallocator)
	FStdMallocator() noexcept
	{
		Type = EMallocator::Std;
	}
	virtual ~FStdMallocator() noexcept final override
	{
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual bool Owned(void*) noexcept final override
	{
		// Trivial, so set to true
		return true;
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual void* Malloc(size_t size) noexcept(false) final override
	{
		return std::malloc(size);
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual void* Malloc2(size_t size) noexcept final override
	{
		try
		{
			return std::malloc(size);
		}
		catch (...)
		{
			return nullptr;
		}
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual void* MallocAligned(size_t size, size_t alignment) noexcept(false) final override
	{
		return std::aligned_alloc(size, alignment);
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual void* MallocAligned2(size_t size, size_t alignment) noexcept final override
	{
		try
		{
			return std::aligned_alloc(size, alignment);
		}
		catch (...)
		{
			return nullptr;
		}
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual EFreeRetType Free(void* ptr) noexcept final override
	{
		std::free(ptr);
		return EFreeRetType::Success;
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual EFreeRetType FreeSize(void* ptr, size_t) noexcept final override
	{
		std::free(ptr);
		return EFreeRetType::Success;
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual EFreeRetType FreeAligned(void* ptr, size_t) noexcept final override
	{
		std::free(ptr);
		return EFreeRetType::Success;
	}
	HLVM_INLINE_FUNC virtual EFreeRetType FreeSizeAligned(void* ptr, size_t, size_t) noexcept final override
	{
		std::free(ptr);
		return EFreeRetType::Success;
	}
};

#if !HLVM_MALLOC_USE_MIMALLOC_OVER_STD
HLVM_THREAD_LOCAL_VAR HLVM_INLINE_VAR FStdMallocator GStdMallocatorTLS{};
	#undef HLVM_LOW_GMALLOC_TLS
	#define HLVM_LOW_GMALLOC_TLS GStdMallocatorTLS
#endif
