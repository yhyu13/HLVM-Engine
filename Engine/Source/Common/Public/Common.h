/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once
#include "GlobalDefinition.h"
#include "Platform/PlatformDefinition.h"
#include "Template/GlobalTemplate.tpp"

// https://github.com/microsoft/mimalloc
#include <mimalloc.h>

HLVM_ENUM(EMallocatorType, uint8_t,
	Mimalloc,
	Std,
	Stack,
	Unkown);

class IMallocator
{
public:
	NOCOPYMOVE(IMallocator)
	IMallocator() = default;
	virtual ~IMallocator() = default;
	HLVM_INLINE_FUNC virtual void* Malloc(size_t size) noexcept(false) = 0;
	HLVM_INLINE_FUNC virtual void* Malloc2(size_t size) noexcept = 0;
	HLVM_INLINE_FUNC virtual void* MallocAligned(size_t size, size_t alignment) noexcept(false) = 0;
	HLVM_INLINE_FUNC virtual void* MallocAligned2(size_t size, size_t alignment) noexcept = 0;
	HLVM_INLINE_FUNC virtual void  Free(void* ptr) noexcept = 0;
	HLVM_INLINE_FUNC virtual void  FreeSize(void* ptr, size_t size) noexcept = 0;
	HLVM_INLINE_FUNC virtual void  FreeAligned(void* ptr, size_t alignment) noexcept = 0;
	HLVM_INLINE_FUNC virtual void  FreeSizeAligned(void* ptr, size_t size, size_t alignment) noexcept = 0;

	EMallocatorType Type = EMallocatorType::Unkown;
};
/**
 * Global mallocator
 */
HLVM_TLS_VAR HLVM_EXTERN_VAR IMallocator* GMallocator;
HLVM_TLS_VAR HLVM_INLINE_VAR IMallocator* GMallocatorSwap = nullptr;
HLVM_INLINE_VAR bool					  GMallocatorSwapped = false;
HLVM_INLINE_FUNC void					  SwapMallocator(IMallocator* Mallocator = nullptr)
{
	GMallocatorSwapped = true;
	if (GMallocatorSwap == nullptr)
	{
		GMallocatorSwap = GMallocator;
		GMallocator = Mallocator;
	}
	else
	{
		GMallocator = GMallocatorSwap;
		GMallocatorSwap = nullptr;
	}
}

class FMiMallocator final : public IMallocator
{
public:
	NOCOPYMOVE(FMiMallocator)
	FMiMallocator()
	{
		Type = EMallocatorType::Mimalloc;
	}
	HLVM_INLINE_FUNC virtual void* Malloc(size_t size) noexcept(false) final override
	{
		return mi_new(size);
	}
	HLVM_INLINE_FUNC virtual void* Malloc2(size_t size) noexcept final override
	{
		return mi_new_nothrow(size);
	}
	HLVM_INLINE_FUNC virtual void* MallocAligned(size_t size, size_t alignment) noexcept(false) final override
	{
		return mi_new_aligned(size, alignment);
	}
	HLVM_INLINE_FUNC virtual void* MallocAligned2(size_t size, size_t alignment) noexcept final override
	{
		return mi_new_aligned_nothrow(size, alignment);
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
};
HLVM_INLINE_VAR FMiMallocator GMiMllocator = FMiMallocator();

class FStdMallocator final : public IMallocator
{
public:
	NOCOPYMOVE(FStdMallocator)
	FStdMallocator()
	{
		Type = EMallocatorType::Std;
	}
	HLVM_INLINE_FUNC virtual void* Malloc(size_t size) noexcept(false) final override
	{
		return std::malloc(size);
	}
	HLVM_INLINE_FUNC virtual void* Malloc2(size_t size) noexcept final override
	{
		try
		{
			return Malloc(size);
		}
		catch (...)
		{
			return nullptr;
		}
	}
	HLVM_INLINE_FUNC virtual void* MallocAligned(size_t size, size_t alignment) noexcept(false) final override
	{
		return std::aligned_alloc(alignment, size);
	}
	HLVM_INLINE_FUNC virtual void* MallocAligned2(size_t size, size_t alignment) noexcept final override
	{
		try
		{
			return MallocAligned(size, alignment);
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
	HLVM_INLINE_FUNC virtual void FreeSize(void* ptr, size_t) noexcept final override
	{
		Free(ptr);
		// cpp 23, not implemented by clang 16 yet https://en.cppreference.com/w/c/memory
		// std::free_sized(ptr, size);
	}
	HLVM_INLINE_FUNC virtual void FreeAligned(void* ptr, size_t) noexcept final override
	{
		Free(ptr);
		// not implemented by clang 16 yet https://en.cppreference.com/w/c/memory
	}
	HLVM_INLINE_FUNC virtual void FreeSizeAligned(void* ptr, size_t, size_t) noexcept final override
	{
		Free(ptr);
		// cpp 23, not implemented by clang 16 yet https://en.cppreference.com/w/c/memory
		// std::free_aligned_sized(ptr, alignment, size);
	}
};
HLVM_INLINE_VAR FStdMallocator GStdMllocator = FStdMallocator();
