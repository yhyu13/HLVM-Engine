/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#define HLVM_NOT_IMPLEMENTED() \
	do                         \
	{                          \
		int* _ = nullptr;      \
		*_ = 0;                \
	}                          \
	while (0)

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
