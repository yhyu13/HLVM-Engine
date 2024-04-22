/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

/**
 * https://compiler-explorer.com/z/5GsaxMe4h
 * x86-64 asm
 *      mov     qword ptr [rbp - 8], 0
		mov     rax, qword ptr [rbp - 8]
		mov     dword ptr [rax], 0
 */
#define HLVM_SEGFAULT_INLINE() \
	do                         \
	{                          \
		int* _ = nullptr;      \
		*_ = 0;                \
	}                          \
	while (0)

#define HLVM_NOT_IMPLEMENTED() HLVM_SEGFAULT_INLINE()

#define HLVM_DELETE(ptr)    \
	do                      \
	{                       \
		if (ptr != nullptr) \
		{                   \
			delete ptr;     \
			ptr = nullptr;  \
		}                   \
	}                       \
	while (0)

#define HLVM_CONSTEXPR_ASSERT(cond, x) \
	do                                 \
	{                                  \
		if constexpr ((cond))          \
		{                              \
			assert((x));               \
		}                              \
	}                                  \
	while (0)
