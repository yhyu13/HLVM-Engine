#pragma once

#if defined(_WIN32)
	#define HLVM_DEBUG_BREAK() __debugbreak()

	#define MS_ALIGN8 __declspec(align(8))
	#define MS_ALIGN16 __declspec(align(16))
	#define MS_ALIGN32 __declspec(align(32))
	#define MS_ALIGN64 __declspec(align(64))
	#define GCC_ALIGN8
	#define GCC_ALIGN16
	#define GCC_ALIGN32
	#define GCC_ALIGN64

	#define HLVM_PLATFORM_CACHE_LINE 64
	#define HLVM_CACHE_ALIGN __declspec(align(PLATFORM_CACHE_LINE))

#elif defined(__linux)
	#include <signal.h>
	#define HLVM_DEBUG_BREAK() raise(SIGTRAP)

	#define MS_ALIGN8
	#define MS_ALIGN16
	#define MS_ALIGN32
	#define MS_ALIGN64
	#define GCC_ALIGN8 __attribute__((packed)) __attribute__((aligned(16))
	#define GCC_ALIGN16 __attribute__((packed)) __attribute__((aligned(16))
	#define GCC_ALIGN32 __attribute__((packed)) __attribute__((aligned(16))
	#define GCC_ALIGN64 __attribute__((packed)) __attribute__((aligned(16))

	#define HLVM_PLATFORM_CACHE_LINE 128
	#define HLVM_CACHE_ALIGN alignas(HLVM_PLATFORM_CACHE_LINE)

#else
	#error "Not implemented for uknown platform"
#endif