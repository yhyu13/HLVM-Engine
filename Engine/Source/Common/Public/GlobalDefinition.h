/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#ifndef HLVM_BUILD_DEBUG
	#define HLVM_BUILD_DEBUG 0
#endif

#ifndef HLVM_BUILD_DEVELOPMENT
	#define HLVM_BUILD_DEVELOPMENT 0
#endif

#ifndef HLVM_BUILD_RELEASE
	#define HLVM_BUILD_RELEASE 0
#endif

#if HLVM_BUILD_RELEASE + HLVM_BUILD_DEBUG + HLVM_BUILD_DEVELOPMENT != 1
	#error "HLVM_BUILD_RELEASE + HLVM_BUILD_DEBUG + HLVM_BUILD_DEVELOPMENT != 1"
#endif

#define HLVM_INLINE_FUNC inline
#define HLVM_STATIC_FUNC static
#define HLVM_EXTERN_FUNC extern
#define HLVM_EXTERN_VAR extern
#define HLVM_INLINE_VAR inline
#define HLVM_STATIC_VAR static
#define HLVM_TLS_VAR thread_local

#define HLVM_UNLIKELY [[unlikely]]
#define HLVM_LIKELY [[likely]]
#define HLVM_NORETURN [[noreturn]]
#define HLVM_NODISCARD [[nodiscard]]
#define HLVM_MAYBEUNUSED [[maybe_unused]]

// 定义一个类，禁止复制和移动
#define NOCOPY(Class)             \
	Class(const Class&) = delete; \
	Class& operator=(const Class&) = delete;

#define NOMOVE(Class)              \
	Class(const Class&&) = delete; \
	Class& operator=(const Class&&) = delete;

#define NOCOPYMOVE(Class) \
	NOCOPY(Class)         \
	NOMOVE(Class)

// 定义一个类，禁止实例化
#define NOINSTANT(Class)                     \
	Class() = delete;                        \
	~Class() = delete;                       \
	Class(const Class&) = delete;            \
	Class(const Class&&) = delete;           \
	Class& operator=(const Class&) = delete; \
	Class& operator=(const Class&&) = delete;

#define TOKENPASTE(x, y) x##y
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)
#define PADDING(size) TBYTE TOKENPASTE2(__padding_, __LINE__)[size]

#define BIT_FLAG(x) bool x : 1

#define HLVM_ENUM(enum_class, type, ...) \
	enum class enum_class : type         \
	{                                    \
		__VA_ARGS__,                     \
		HLVM_NUM                         \
	};                                   \
	HLVM_INLINE_VAR constexpr size_t enum_class##_NUM = static_cast<size_t>(enum_class::HLVM_NUM)

#define HLVM_ENUM_V(enum_class, enum_value) static_cast<std::underlying_type_t<enum_class>>(enum_class::enum_value)
#define HLVM_ENUM_SIZE_T(enum_value) static_cast<size_t>((enum_value))

#define S_C(type, value) static_cast<type>((value))
#define SP_C(type, value) static_pointer_cast<type>((value))
#define D_C(type, value) dynamic_cast<type>((value))
#define C_C(type, value) const_cast<type>((value))
#define R_C(type, value) reinterpret_cast<type>((value))

#define HLVM_NOT_IMPLEMENTED() assert(false)

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

// Use char for best compatibility with other libraries
#define TCHAR char8_t
static_assert(sizeof(TCHAR) == sizeof(char), "TCHAR is not char in size");

//  Use utf8 for all string literal
//  U8_STRING("Hello World!")
#define U8_STRING(str) u8##str
#define TXT(str) U8_STRING(str)
#define STRTIFY(x) TXT(#x)
#define TO_TCHAR_STR(x) reinterpret_cast<const TCHAR*>((x))
#define TO_CHAR_STR(x) reinterpret_cast<const char*>((x))

#define HLVM_ENUM_TCHAR_STR(value) TO_TCHAR_STR(magic_enum::enum_name((value)).data())

#define TBYTE std::byte
static_assert(sizeof(TBYTE) == sizeof(char), "TBYTE is not char in size");
#define TUINT8 std::uint8_t
static_assert(sizeof(TUINT8) == sizeof(char), "TUINT8 is not char in size");
#define TUINT16 std::uint16_t
static_assert(sizeof(TUINT16) == 2 * sizeof(char), "uint16_t is not char in size");
#define TUINT32 std::uint32_t
static_assert(sizeof(TUINT32) == 4 * sizeof(char), "uint32_t is not char in size");

#define HLVM_CONSTEXPR_ASSERT(cond, x) \
	do                                 \
	{                                  \
		if constexpr ((cond))          \
		{                              \
			assert((x));               \
		}                              \
	}                                  \
	while (0)

#include "UserPredefined.gen.h"
