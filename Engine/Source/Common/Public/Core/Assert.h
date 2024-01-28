#pragma once

#include "Platform/PlatformDefinition.h"
#include "Log.h"

#ifndef HLVM_ASSERT_EVEN_IN_RELEASE
	#define HLVM_ASSERT_EVEN_IN_RELEASE 0
#endif

#if !HLVM_BUILD_RELEASE
	#define ASSERT(x, ...)                                                                                                                     \
		do                                                                                                                                     \
		{                                                                                                                                      \
			if (static_cast<bool>((x)) == false)                                                                                               \
			{                                                                                                                                  \
				HLVM_DEBUG_BREAK();                                                                                                            \
				HLVM_LOG(LogTemp, critical, TXT("Assertion failed: {3}, {2} at {0}:{1}"), TXT(__FILE__), __LINE__, STRTIFY(x), ##__VA_ARGS__); \
			}                                                                                                                                  \
		}                                                                                                                                      \
		while (0)
#else
	#if HLVM_ASSERT_EVEN_IN_RELEASE
		#define ASSERT(x, ...)                                                                                                                \
			do                                                                                                                                \
			{                                                                                                                                 \
				if (static_cast<bool>((x)) == false)                                                                                          \
				{                                                                                                                             \
					HLVM_LOG(LogTemp, critical, "Assertion failed: {3}, {2} at {0}:{1}", TXT(__FILE__), __LINE__, STRTIFY(x), ##__VA_ARGS__); \
				}                                                                                                                             \
			}                                                                                                                                 \
			while (0)
	#else
		#define ASSERT(x, ...)
	#endif // DEBUG
#endif