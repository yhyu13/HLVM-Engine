/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"
#include "MallocatorDefinition.h"

#include <cstddef>

HLVM_ENUM(EMallocator, TUINT8,
	Std,
	Mimalloc,
	Stack,
	VMArena,
	Unknow);

HLVM_ENUM(EFreeRetType, TUINT8,
	Success,
	NotOwned,
	Fail);

/**
 * Mallocator interface class
 * Default allocator is Mimalloc
 */
class IMallocator
{
public:
	NOCOPYMOVE(IMallocator)
	IMallocator() noexcept = default;
	virtual ~IMallocator() noexcept = default;
	HLVM_NODISCARD virtual bool			Owned(void* ptr) noexcept = 0;
	HLVM_NODISCARD virtual void*		Malloc(size_t size) noexcept(false) = 0;
	HLVM_NODISCARD virtual void*		Malloc2(size_t size) noexcept = 0;
	HLVM_NODISCARD virtual void*		MallocAligned(size_t size, size_t alignment) noexcept(false) = 0;
	HLVM_NODISCARD virtual void*		MallocAligned2(size_t size, size_t alignment) noexcept = 0;
	HLVM_NODISCARD virtual EFreeRetType Free(void* ptr) noexcept = 0;
	HLVM_NODISCARD virtual EFreeRetType FreeSize(void* ptr, size_t size) noexcept = 0;
	HLVM_NODISCARD virtual EFreeRetType FreeAligned(void* ptr, size_t alignment) noexcept = 0;
	HLVM_NODISCARD virtual EFreeRetType FreeSizeAligned(void* ptr, size_t size, size_t alignment) noexcept = 0;

	/**
	 * Collect memory (optional feature), inspired by below mimalloc bug to collect memory in thread pools
	 * https://pwy.io/posts/mimalloc-cigarette/
	 * https://microsoft.github.io/mimalloc/group__extended.html#ga421430e2226d7d468529cec457396756
	 * It can be beneficial in very narrow circumstances;
	 * in particular, when a long running thread allocates a lot of blocks that are freed by other threads it may improve resource usage by calling this every once in a while.
	 */
	virtual void Collect(bool bForce = false) noexcept {}

public:
	EMallocator Type = EMallocator::Unknow;
};
/**
 * Global mallocator
 */
HLVM_EXTERN_FUNC void							   InitMallocator();
HLVM_EXTERN_FUNC void							   FinlMallocator();
HLVM_EXTERN_FUNC void							   SwapMallocator(IMallocator* Mallocator = nullptr);
HLVM_THREAD_LOCAL_VAR HLVM_EXTERN_VAR IMallocator* GMallocatorTLS;
HLVM_THREAD_LOCAL_VAR HLVM_EXTERN_VAR IMallocator* GFallBacllMallocatorTLS;
namespace hlvm_private
{
	HLVM_THREAD_LOCAL_VAR HLVM_INLINE_VAR IMallocator* GMallocatorTLSSwap = nullptr;
}

/**
 * Global mallocator for low level mallocation, defined at compile time
 * which is different from GmallocatorTLS which can be swapped during runtime
 */
#define HLVM_LOW_GMALLOC_TLS ((void)0)
