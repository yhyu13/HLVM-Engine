/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

/**
 * Platform check macros : https://github.com/abseil/abseil.github.io/blob/master/docs/cpp/platforms/macros.md
 */
#if defined(_WIN32) && defined(_MSC_VER)
	#define PLATFORM_WINDOWS
#endif

#if defined(__linux__) && defined(__GNUC__)
	#define PLATFORM_LINUXGNU
#endif

#if defined(PLATFORM_WINDOWS)
	#define HLVM_DEBUG_BREAK() __debugbreak()

	#define PACK(__class__) __pragma(pack(push, 1)) __class__ __pragma(pack(pop))
	#define MS_ALIGN(N) __declspec(align(N))
	#define GCC_ALIGN(N)

	#define HLVM_PLATFORM_CACHE_LINE 64
	#define HLVM_CACHE_ALIGN __declspec(align(PLATFORM_CACHE_LINE))

#elif defined(PLATFORM_LINUXGNU)
	#include <signal.h>
	#define HLVM_DEBUG_BREAK() raise(SIGTRAP)

	#define PACK(__class__) __class__ __attribute__((__packed__))
	#define MS_ALIGN(N)
	#define GCC_ALIGN(N) __attribute__((aligned(N)))

	#define HLVM_PLATFORM_CACHE_LINE 64
	#define HLVM_CACHE_ALIGN alignas(HLVM_PLATFORM_CACHE_LINE)

#else
	#error "Not implemented for uknown platform"
#endif

/**
 *  This macro is defined for desktop platforms but not for mobile platforms like Android or iOS
 */
#ifdef __STDCPP_DEFAULT_NEW_ALIGNMENT__
	#define PLATFORM_DESKTOP 1
#else
	#define PLATFORM_DESKTOP 0
#endif

/**
 * Generic platform definitions
 */
static_assert(1 == sizeof(char), "char is not 1 byte in size");
static_assert(1 == sizeof(uint8_t), "char is not 1 byte in size");
static_assert(1 == sizeof(std::byte), "char is not 1 byte in size");
