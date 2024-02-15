/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/String.h"
#include "Template/ExpressionTemplate.tpp"

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
constexpr auto __ctre_checkExpressionPassAssert(std::u8string_view sv) noexcept -> bool
{
	return ctre_MatchFunctionCall(sv) || ctre_MatchAssignment(sv);
};
		#define HLVM_ASSERT(x, ...) \
			static_assert(!(__ctre_checkExpressionPassAssert(STRTIFY(x))), "Should not ignore evaluation of this expression, consider set HLVM_ASSERT_ALWAYS_EVLUATE_EXPERSION=1 or using HLVM_ENSURE")
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
