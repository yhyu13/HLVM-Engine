/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "Core/String.h"
#include "Core/Parallel/Lock.h"
#include "Template/ExpressionTemplate.tpp"
#include "Platform/PlatformDefinition.h"

#include "Core/Mallocator/IMallocator.h"
#include "Template/FunctionTemplate.tpp"

namespace hlvm_private
{
	HLVM_EXTERN_VAR FAtomicFlagNC AssertionStackLock;
	HLVM_EXTERN_VAR IMallocator*  AssertionStackMallocator;
	HLVM_EXTERN_FUNC void		  InitAssertionStackMallocator();

	HLVM_NORETURN HLVM_NOINLINE_FUNC void
	hlvm_internal_assert(const TCHAR* Expression, const FString* Message, const TCHAR* File, int Line);
} // namespace hlvm_private

#ifndef HLVM_ASSERT_EVEN_IN_RELEASE
	#define HLVM_ASSERT_EVEN_IN_RELEASE 0
#endif

#ifndef HLVM_ASSERT_ALWAYS_EVLUATE_EXPERSION
	/// @brief 是否总是执行断言的表达式，即使发布模式下，默认为0，即不执行断言的表达式。
	#define HLVM_ASSERT_ALWAYS_EVLUATE_EXPERSION 0
#endif

// Assert is not always available and the argument 'x' would not be evaluated when assert is disabled
/**
 * 断言：开发模式下，判断条件是否成立，不满足则抛出异常。发布环境下，断言不生效。
 * 条件大多数情况下就是简单的表达式，如：x == 100，x > 100，x < 100。发布环境下，判断条件被省略，可以避免不必要的计算。
 */
#if !HLVM_BUILD_RELEASE || HLVM_ASSERT_EVEN_IN_RELEASE
	#define HLVM_ASSERT(x, ...)                                                                                                       \
		do                                                                                                                            \
		{                                                                                                                             \
			if (static_cast<bool>((x)) == false)                                                                                      \
			{                                                                                                                         \
				ATOMIC_LOCK_GUARD(hlvm_private::AssertionStackLock);                                                                  \
				hlvm_private::InitAssertionStackMallocator();                                                                         \
				SwapMallocator(hlvm_private::AssertionStackMallocator);                                                               \
				hlvm_private::hlvm_internal_assert(STRTIFY(x), new FString{ FString::Format(__VA_ARGS__) }, TXT(__FILE__), __LINE__); \
			}                                                                                                                         \
		}                                                                                                                             \
		while (0)
	#define HLVM_ASSERT2(x) HLVM_ASSERT(x, TXT("condition failed: {}"), STRTIFY(x))
#else
	#if HLVM_ASSERT_ALWAYS_EVLUATE_EXPERSION
		#define HLVM_ASSERT(x, ...)    \
			do                         \
			{                          \
				static_cast<bool>((x)) \
			}                          \
			while (0)
		#define HLVM_ASSERT2(x) HLVM_ASSERT(x, TXT("condition failed: {}"), STRTIFY(x))
	#else
namespace hlvm_private
{
	constexpr auto ctre_checkExpressionPassAssert(std::u8string_view sv) noexcept -> bool
	{
		return ctre_MatchFunctionCall(sv) || ctre_MatchAssignment(sv);
	};
} // namespace hlvm_private
		#define HLVM_ASSERT(x, ...) \
			static_assert(!(hlvm_private::ctre_checkExpressionPassAssert(STRTIFY(x))), "Should not ignore evaluation of this expression, consider set HLVM_ASSERT_ALWAYS_EVLUATE_EXPERSION=1 or using HLVM_ENSURE")
		#define HLVM_ASSERT2(x, ...) \
			static_assert(!(hlvm_private::ctre_checkExpressionPassAssert(STRTIFY(x))), "Should not ignore evaluation of this expression, consider set HLVM_ASSERT_ALWAYS_EVLUATE_EXPERSION=1 or using HLVM_ENSURE2")
	#endif
#endif

// Ensure is always available in the argument 'x' is evaluated
/**
 * 确保：开发与发布模式下，判断语句返回值是否为真，否则抛出异常
 * 语句大多数是函数执行结果，如：x() == 100，x() > 100，x() < 100。如果x()不计算会影响到程序的正确性，则不能用断言
 */
#define HLVM_ENSURE(x, ...)                                                                                                       \
	do                                                                                                                            \
	{                                                                                                                             \
		if (static_cast<bool>((x)) == false)                                                                                      \
		{                                                                                                                         \
			ATOMIC_LOCK_GUARD(hlvm_private::AssertionStackLock);                                                                  \
			hlvm_private::InitAssertionStackMallocator();                                                                         \
			SwapMallocator(hlvm_private::AssertionStackMallocator);                                                               \
			hlvm_private::hlvm_internal_assert(STRTIFY(x), new FString{ FString::Format(__VA_ARGS__) }, TXT(__FILE__), __LINE__); \
		}                                                                                                                         \
	}                                                                                                                             \
	while (0)
#define HLVM_ENSURE2(x) HLVM_ENSURE(x, TXT("condition failed: {}"), STRTIFY(x))
