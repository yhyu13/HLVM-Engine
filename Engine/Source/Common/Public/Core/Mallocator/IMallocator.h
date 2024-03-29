/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"
#include "MallocatorDefinition.h"

#include <cstddef>

HLVM_ENUM(EMallocator, TUINT8,
	Mimalloc,
	Stack,
	VirtualMemory,
	Unkown);

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
	HLVM_INLINE_FUNC virtual bool  Owened(void* ptr) noexcept = 0;
	HLVM_INLINE_FUNC virtual void* Malloc(std::size_t size) noexcept(false) = 0;
	HLVM_INLINE_FUNC virtual void* Malloc2(std::size_t size) noexcept = 0;
	HLVM_INLINE_FUNC virtual void* MallocAligned(std::size_t size, std::size_t alignment) noexcept(false) = 0;
	HLVM_INLINE_FUNC virtual void* MallocAligned2(std::size_t size, std::size_t alignment) noexcept = 0;
	HLVM_INLINE_FUNC virtual void  Free(void* ptr) noexcept = 0;
	HLVM_INLINE_FUNC virtual void  FreeSize(void* ptr, std::size_t size) noexcept = 0;
	HLVM_INLINE_FUNC virtual void  FreeAligned(void* ptr, std::size_t alignment) noexcept = 0;
	HLVM_INLINE_FUNC virtual void  FreeSizeAligned(void* ptr, std::size_t size, std::size_t alignment) noexcept = 0;

public:
	EMallocator Type = EMallocator::Unkown;
};
/**
 * Global mallocator
 */
void									  InitMallocator();
void									  SwapMallocator(IMallocator* Mallocator = nullptr);
HLVM_TLS_VAR HLVM_EXTERN_VAR IMallocator* GMallocatorTLS;
namespace hlvm_private
{
	HLVM_TLS_VAR HLVM_INLINE_VAR IMallocator* GMallocatorTLSSwap = nullptr;
}
