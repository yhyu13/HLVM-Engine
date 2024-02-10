/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Platform/GenericPlatformDebuggerUtil.h"
#include "Core/Log.h"
#include "Core/String.h"

#include <exception>

#ifndef HLVM_ASSERT_EVEN_IN_RELEASE
	#define HLVM_ASSERT_EVEN_IN_RELEASE 0
#endif

#if !HLVM_BUILD_RELEASE
	#define HLVM_ASSERT(x, ...)                                                                                                                         \
		do                                                                                                                                              \
		{                                                                                                                                               \
			if (static_cast<bool>((x)) == false)                                                                                                        \
			{                                                                                                                                           \
				const FCharStringView& StackTrace = FGenericPlatformDebuggerUtil::GetStackTrace();                                                      \
				const FString&		   msg = FString::Format(TXT("Assertion failed: {1}, with '{2}' at\n{0}"), *StackTrace, STRTIFY(x), ##__VA_ARGS__); \
				HLVM_LOG(LogTemp, critical, *msg);                                                                                                      \
				HLVM_TRY_DEBUG_BREAK();                                                                                                                 \
				throw std::runtime_error(msg.ToCharStr());                                                                                              \
			}                                                                                                                                           \
		}                                                                                                                                               \
		while (0)
#else
	#if HLVM_ALLOW_ASSERT_EVEN_IN_RELEASE
		#define HLVM_ASSERT(x, ...)                                                                                                                         \
			do                                                                                                                                              \
			{                                                                                                                                               \
				if (static_cast<bool>((x)) == false)                                                                                                        \
				{                                                                                                                                           \
					const FCharStringView& StackTrace = FGenericPlatformDebuggerUtil::GetStackTrace();                                                      \
					const FString&		   msg = FString::Format(TXT("Assertion failed: {1}, with '{2}' at\n{0}"), *StackTrace, STRTIFY(x), ##__VA_ARGS__); \
					HLVM_LOG(LogTemp, critical, *msg);                                                                                                      \
					HLVM_TRY_DEBUG_BREAK();                                                                                                                 \
					throw std::runtime_error(msg.ToCharStr());                                                                                              \
				}                                                                                                                                           \
			}                                                                                                                                               \
			while (0)
	#else
		#define HLVM_ASSERT(...)
	#endif // DEBUG
#endif

#define HLVM_ENSURE(x, ...)                                                                                                                      \
	do                                                                                                                                           \
	{                                                                                                                                            \
		if (static_cast<bool>((x)) == false)                                                                                                     \
		{                                                                                                                                        \
			const FCharStringView& StackTrace = FGenericPlatformDebuggerUtil::GetStackTrace();                                                   \
			const FString&		   msg = FString::Format(TXT("Ensure failed: {1}, with '{2}' at\n{0}"), *StackTrace, STRTIFY(x), ##__VA_ARGS__); \
			HLVM_LOG(LogTemp, critical, *msg);                                                                                                   \
			HLVM_TRY_DEBUG_BREAK();                                                                                                              \
			throw std::runtime_error(msg.ToCharStr());                                                                                           \
		}                                                                                                                                        \
	}                                                                                                                                            \
	while (0)