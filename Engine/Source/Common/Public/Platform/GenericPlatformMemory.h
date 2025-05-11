/**
* Copyright (c) 2025. MIT License. All rights reserved.
*/

#pragma once

#include "GenericPlatform.h"
#include <memory>

class FGenericPlatformMemory
{
public:
   HLVM_INLINE_FUNC HLVM_STATIC_FUNC void* Memzero(void* Dest, TSIZE Size)
   {
	   std::memset(Dest, 0, Size);
	   return Dest;
   }

   // Memcmp
   HLVM_INLINE_FUNC HLVM_STATIC_FUNC TINT64 Memcmp(const void* Src1, const void* Src2, TSIZE Size)
   {
	   return std::memcmp(Src1, Src2, Size);
   }

   // Memset
   HLVM_INLINE_FUNC HLVM_STATIC_FUNC void* Memset(void* Dest, TINT32 Char, TSIZE Size)
   {
	   std::memset(Dest, Char, Size);
	   return Dest;
   }
};
