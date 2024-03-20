/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"
#include "MallocatorDefinition.h"

HLVM_ENUM(EMallocator, TUINT8,
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
void									  SwapMallocator(IMallocator* Mallocator = nullptr);
HLVM_TLS_VAR HLVM_EXTERN_VAR IMallocator* GMallocatorTLS;
namespace hlvm_private
{
	HLVM_TLS_VAR HLVM_INLINE_VAR IMallocator* GMallocatorTLSSwap = nullptr;
}
