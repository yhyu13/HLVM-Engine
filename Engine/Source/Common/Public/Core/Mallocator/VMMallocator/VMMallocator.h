/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "VMArena.h"

class FVMArenaMallocator final : public IMallocator
{
public:
	NOCOPYMOVE(FVMArenaMallocator)
	FVMArenaMallocator() noexcept
	{
		Type = EMallocator::VMArena;
	}
	virtual ~FVMArenaMallocator() noexcept final override
	{
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual bool Owned(void* p) noexcept final override
	{
		return Arena.Owned(p);
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual void* Malloc(size_t size) noexcept(false) final override
	{
		return Arena.Malloc(size);
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual void* Malloc2(size_t size) noexcept final override
	{
		try
		{
			return Arena.Malloc(size);
		}
		catch (...)
		{
			return nullptr;
		}
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual void* MallocAligned(size_t size, size_t alignment) noexcept(false) final override
	{
		(void)alignment;
		return Arena.Malloc(size);
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual void* MallocAligned2(size_t size, size_t alignment) noexcept final override
	{
		try
		{
			(void)alignment;
			return Arena.Malloc(size);
		}
		catch (...)
		{
			return nullptr;
		}
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual EFreeRetType Free(void* ptr) noexcept final override
	{
		Arena.Free(ptr);
		return EFreeRetType::Success;
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual EFreeRetType FreeSize(void* ptr, size_t) noexcept final override
	{
		Arena.Free(ptr);
		return EFreeRetType::Success;
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual EFreeRetType FreeAligned(void* ptr, size_t) noexcept final override
	{
		Arena.Free(ptr);
		return EFreeRetType::Success;
	}
	HLVM_INLINE_FUNC virtual EFreeRetType FreeSizeAligned(void* ptr, size_t, size_t) noexcept final override
	{
		Arena.Free(ptr);
		return EFreeRetType::Success;
	}

private:
	FVMArena Arena{};
};
