/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"
#include "MallocatorDefinition.h"

#include <cstddef>

HLVM_ENUM(EMallocator, TUINT8,
	Std,
	Mimalloc,
	Stack,
	Unkown);

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
	HLVM_NODISCARD virtual void*		Malloc(std::size_t size) noexcept(false) = 0;
	HLVM_NODISCARD virtual void*		Malloc2(std::size_t size) noexcept = 0;
	HLVM_NODISCARD virtual void*		MallocAligned(std::size_t size, std::size_t alignment) noexcept(false) = 0;
	HLVM_NODISCARD virtual void*		MallocAligned2(std::size_t size, std::size_t alignment) noexcept = 0;
	HLVM_NODISCARD virtual EFreeRetType Free(void* ptr) noexcept = 0;
	HLVM_NODISCARD virtual EFreeRetType FreeSize(void* ptr, std::size_t size) noexcept = 0;
	HLVM_NODISCARD virtual EFreeRetType FreeAligned(void* ptr, std::size_t alignment) noexcept = 0;
	HLVM_NODISCARD virtual EFreeRetType FreeSizeAligned(void* ptr, std::size_t size, std::size_t alignment) noexcept = 0;

public:
	EMallocator Type = EMallocator::Unkown;
};
/**
 * Global mallocator
 */
HLVM_EXTERN_FUNC void							   InitMallocator();
HLVM_EXTERN_FUNC void							   FnalMallocator();
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
#define HLVM_LOWLVL_GMALLOCATOR ((void)0)
