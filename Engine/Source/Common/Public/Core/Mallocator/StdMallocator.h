/**
 * Copyright (c) 2024. MIT License. All rights reserved.
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
	~FStdMallocator() noexcept final override
	{
	}
	virtual bool Owned(void*) noexcept final override
	{
		// Trivial, so set to false
		return false;
	}
	HLVM_INLINE_FUNC virtual void* Malloc(std::size_t size) noexcept(false) final override
	{
		return std::malloc(size);
	}
	HLVM_INLINE_FUNC virtual void* Malloc2(std::size_t size) noexcept final override
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
	HLVM_INLINE_FUNC virtual void* MallocAligned(std::size_t size, std::size_t alignment) noexcept(false) final override
	{
		return std::aligned_alloc(size, alignment);
	}
	HLVM_INLINE_FUNC virtual void* MallocAligned2(std::size_t size, std::size_t alignment) noexcept final override
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
	HLVM_INLINE_FUNC virtual void Free(void* ptr) noexcept final override
	{
		std::free(ptr);
	}
	HLVM_INLINE_FUNC virtual void FreeSize(void* ptr, std::size_t) noexcept final override
	{
		std::free(ptr);
	}
	HLVM_INLINE_FUNC virtual void FreeAligned(void* ptr, std::size_t) noexcept final override
	{
		std::free(ptr);
	}
	HLVM_INLINE_FUNC virtual void FreeSizeAligned(void* ptr, std::size_t, std::size_t) noexcept final override
	{
		std::free(ptr);
	}
};
HLVM_INLINE_VAR FStdMallocator GStdMallocator{};
