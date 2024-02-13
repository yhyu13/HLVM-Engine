/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Platform/GenericPlatformDebuggerUtil.h"
#include "Core/Log.h"
#include "Core/String.h"

#include <exception>

[[noreturn]] void hlvm_internal_assert(const TCHAR* Expression, FString&& Message, const TCHAR* File, int Line);

#ifndef HLVM_ASSERT_EVEN_IN_RELEASE
	#define HLVM_ASSERT_EVEN_IN_RELEASE 0
#endif

#ifndef HLVM_ASSERT_ALWAYS_EVLUATE_EXPERSION
	#define HLVM_ASSERT_ALWAYS_EVLUATE_EXPERSION 0
#endif

// Assert is not always available and the argument 'x' would not be evaluated when assert is disabled
#if !HLVM_BUILD_RELEASE || HLVM_ASSERT_EVEN_IN_RELEASE
	#define HLVM_ASSERT(x, ...)                                                                          \
		do                                                                                               \
		{                                                                                                \
			if (static_cast<bool>((x)) == false)                                                         \
				hlvm_internal_assert(STRTIFY(x), FString::Format(__VA_ARGS__), TXT(__FILE__), __LINE__); \
		}                                                                                                \
		while (0)
#else
	#if HLVM_ASSERT_ALWAYS_EVLUATE_EXPERSION
		#define HLVM_ASSERT(x, ...)    \
			do                         \
			{                          \
				static_cast<bool>((x)) \
			}                          \
			while (0)
	#else
		#define HLVM_ASSERT(x, ...) \
			static_assert(!(ctre_MatchFunctionCall(STRTIFY(x)) || ctre_MatchAssignment(STRTIFY(x))), "Should not ignore evaluation of this expression, consider set HLVM_ASSERT_ALWAYS_EVLUATE_EXPERSION=1 or using HLVM_ENSURE")
	#endif
#endif

// Ensure is always available in the argument 'x' is evaluated
#define HLVM_ENSURE(x, ...)                                                                          \
	do                                                                                               \
	{                                                                                                \
		if (static_cast<bool>((x)) == false)                                                         \
			hlvm_internal_assert(STRTIFY(x), FString::Format(__VA_ARGS__), TXT(__FILE__), __LINE__); \
	}                                                                                                \
	while (0)

// Avoid using this inline macro as it creates too many inline code
// #define HLVM_ENSURE(x, ...)                                                                                                                      \
//	do                                                                                                                                           \
//	{                                                                                                                                            \
//		if (static_cast<bool>((x)) == false)                                                                                                     \
//		{                                                                                                                                        \
//			const FCharStringView& StackTrace = FGenericPlatformDebuggerUtil::GetStackTrace();                                                   \
//			const FString&		   msg = FString::Format(TXT("Ensure failed: {1}, with '{2}' at\n{0}"), *StackTrace, STRTIFY(x), __VA_ARGS__); \
//			HLVM_LOG(LogAssert, critical, *msg);                                                                                                 \
//			HLVM_TRY_DEBUG_BREAK();                                                                                                              \
//			throw std::runtime_error(msg.ToCharStr());                                                                                           \
//		}                                                                                                                                        \
//	}                                                                                                                                            \
//	while (0)
