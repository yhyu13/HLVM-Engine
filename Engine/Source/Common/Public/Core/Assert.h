/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Platform/GenericPlatformDebuggerUtil.h"
#include "Log.h"

#include <exception>

#ifndef HLVM_ASSERT_EVEN_IN_RELEASE
	#define HLVM_ASSERT_EVEN_IN_RELEASE 0
#endif

// TODO: use async exception handling here, and some stack trace
#if !HLVM_BUILD_RELEASE
	#define HLVM_ASSERT(x, ...)                                                                                                                     \
		do                                                                                                                                          \
		{                                                                                                                                           \
			if (static_cast<bool>((x)) == false)                                                                                                    \
			{                                                                                                                                       \
				FString msg = fmt::format(TXT("Assertion failed: {3}, with '{2}' at {0}:{1}"), TXT(__FILE__), __LINE__, STRTIFY(x), ##__VA_ARGS__); \
				HLVM_LOG(LogTemp, critical, msg);                                                                                                   \
				HLVM_TRY_DEBUG_BREAK();                                                                                                             \
				throw std::runtime_error(msg.ToCharStr());                                                                                          \
			}                                                                                                                                       \
		}                                                                                                                                           \
		while (0)
#else
	#if HLVM_ALLOW_ASSERT_EVEN_IN_RELEASE
		#define HLVM_ASSERT(x, ...)                                                                                                                     \
			do                                                                                                                                          \
			{                                                                                                                                           \
				if (static_cast<bool>((x)) == false)                                                                                                    \
				{                                                                                                                                       \
					FString msg = fmt::format(TXT("Assertion failed: {3}, with '{2}' at {0}:{1}"), TXT(__FILE__), __LINE__, STRTIFY(x), ##__VA_ARGS__); \
					HLVM_LOG(LogTemp, critical, msg);                                                                                                   \
					HLVM_TRY_DEBUG_BREAK();                                                                                                             \
					throw std::runtime_error(msg.ToCharStr());                                                                                          \
				}                                                                                                                                       \
			}                                                                                                                                           \
			while (0)
	#else
		#define HLVM_ASSERT(...)
	#endif // DEBUG
#endif