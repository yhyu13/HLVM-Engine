/**
* Copyright (c) 2025. MIT License. All rights reserved.
*/

#pragma once

#include "Platform/GenericPlatformMemory.h"

class FMemory
{
public:
	HLVM_INLINE_FUNC HLVM_STATIC_FUNC void* Memzero(void* Ptr, size_t Size)
	{
		return FGenericPlatformMemory::Memzero(Ptr, Size);
	}

	template<class T>
	HLVM_STATIC_FUNC void* Memzero(T* Ptr)
	{
		static_assert(!std::is_pointer_v<T>, "Don't use a pointer!");
		return FGenericPlatformMemory::Memzero(&Ptr, sizeof(T));
	}
};
