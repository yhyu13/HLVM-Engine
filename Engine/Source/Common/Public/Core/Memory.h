/**
* Copyright (c) 2026. MIT License. All rights reserved.
*/

#pragma once

#include "Platform/GenericPlatformMemory.h"

class FMemory
{
public:
	HLVM_INLINE_FUNC HLVM_STATIC_FUNC void* Memzero(void* Ptr, TSIZE Size)
	{
		return FGenericPlatformMemory::Memzero(Ptr, Size);
	}

	template<class T>
	HLVM_STATIC_FUNC void* Memzero(T* Ptr)
	{
		static_assert(!std::is_pointer_v<T>, "Don't use a pointer!");
		static_assert(!std::is_array_v<T>, "Don't use a array!");
		return FGenericPlatformMemory::Memzero(Ptr, sizeof(T));
	}

	template<class T>
	HLVM_STATIC_FUNC void* MemzeroArray(T* Ptr)
	{
		static_assert(!std::is_pointer_v<T>, "Don't use a pointer!");
		static_assert(std::is_array_v<T>, "Use a array but not!");
		return FGenericPlatformMemory::Memzero(*Ptr, sizeof(T));
	}

	// Memcmp
	HLVM_INLINE_FUNC HLVM_STATIC_FUNC TINT64 Memcmp(const void* Ptr1, const void* Ptr2, TSIZE Size)
	{
		return FGenericPlatformMemory::Memcmp(Ptr1, Ptr2, Size);
	}
	
	// Memset
	HLVM_INLINE_FUNC HLVM_STATIC_FUNC void* Memset(void* Ptr, TINT32 Value, TSIZE Size)
	{
		return FGenericPlatformMemory::Memset(Ptr, Value, Size);
	}
};
