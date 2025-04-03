/**
* Copyright (c) 2025. MIT License. All rights reserved.
*/

#pragma once

#include "GenericPlatform.h"
#include <memory>

class FGenericPlatformMemory
{
public:
   HLVM_INLINE_FUNC HLVM_STATIC_FUNC void* Memzero(void* Dest, size_t Size)
   {
	   std::memset(Dest, 0, Size);
	   return Dest;
   }
};
