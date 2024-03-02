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
#define PADDING(size) std::byte TOKENPASTE2(padding_, __LINE__)[size]

#define BIT_FLAG(x) bool x : 1

#define HLVM_ENUM(enum_class, type, ...) \
	enum class enum_class : type         \
	{                                    \
		__VA_ARGS__,                     \
		HLVM_NUM                         \
	};                                   \
	HLVM_INLINE_VAR constexpr size_t enum_class##_NUM = static_cast<size_t>(enum_class::HLVM_NUM)

#define HLVM_ENUM_V(enum_class, enum_value) static_cast<std::underlying_type_t<enum_class>>(enum_class::enum_value)
#define HLVM_ENUM_V_SIZE_T(enum_class, enum_value) static_cast<size_t>(enum_class::enum_value)

#define S_C(type, value) static_cast<type>(value)
#define SP_C(type, value) static_pointer_cast<type>(value)
#define D_C(type, value) dynamic_cast<type>(value)
#define C_C(type, value) const_cast<type>(value)
#define R_C(type, value) reinterpret_cast<type>(value)

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
static_assert(sizeof(TCHAR) == sizeof(char), "TCHAR is not char");

//  Use utf8 for all string literal
//  U8_STRING("Hello World!")
#define U8_STRING(str) u8##str
#define TXT(str) U8_STRING(str)
#define STRTIFY(x) TXT(#x)
#define TO_TCHAR_STR(x) reinterpret_cast<const TCHAR*>((x))
#define TO_CHAR_STR(x) reinterpret_cast<const char*>((x))
