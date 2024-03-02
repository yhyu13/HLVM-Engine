/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/String.h"
#include "Template/ExpressionTemplate.tpp"

HLVM_NORETURN void hlvm_internal_assert(const TCHAR* Expression, FString&& Message, const TCHAR* File, int Line);

#ifndef HLVM_ASSERT_EVEN_IN_RELEASE
	#define HLVM_ASSERT_EVEN_IN_RELEASE 0
#endif

#ifndef HLVM_ASSERT_ALWAYS_EVLUATE_EXPERSION
	#define HLVM_ASSERT_ALWAYS_EVLUATE_EXPERSION 0
#endif

// Assert is not always available and the argument 'x' would not be evaluated when assert is disabled
/**
 * 断言：开发模式下，判断条件是否成立，不满足则抛出异常。发布环境下，断言不生效。
 * 条件大多数情况下就是简单的表达式，如：x == 100，x > 100，x < 100。发布环境下，判断条件被省略，可以避免不必要的计算。
 */
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
/**
 * 确保：开发与发布模式下，判断语句返回值是否为真，否则抛出异常
 * 语句大多数是函数执行结果，如：x() == 100，x() > 100，x() < 100。如果x()不计算会影响到程序的正确性，则不能用断言
 */
#define HLVM_ENSURE(x, ...)                                                                          \
	do                                                                                               \
	{                                                                                                \
		if (static_cast<bool>((x)) == false)                                                         \
			hlvm_internal_assert(STRTIFY(x), FString::Format(__VA_ARGS__), TXT(__FILE__), __LINE__); \
	}                                                                                                \
	while (0)
