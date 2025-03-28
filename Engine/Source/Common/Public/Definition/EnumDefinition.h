/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#define HLVM_ENUM(enum_class, type, ...) \
	enum class enum_class : type         \
	{                                    \
		__VA_ARGS__,                     \
		HLVM_NUM                         \
	};                                   \
	HLVM_INLINE_VAR constexpr size_t enum_class##_NUM = static_cast<size_t>(enum_class::HLVM_NUM)

#define HLVM_ENUM_V(enum_class, enum_value) static_cast<std::underlying_type_t<enum_class>>(enum_class::enum_value)
#define HLVM_ENUM_SIZE_T(enum_value) static_cast<size_t>((enum_value))

#define HLVM_ENUM_TCHAR_STR(value) TO_TCHAR_CSTR(magic_enum::enum_name((value)).data())

#define HLVM_ENUM_BITWISE_OPERATOR_DEFINE(enum_class, operator_type)                                                                              \
	HLVM_INLINE_FUNC bool operator operator_type(const enum_class& lhs, const enum_class& rhs)                                          \
	{                                                                                                                                   \
		return static_cast<std::underlying_type_t<enum_class>>(lhs) operator_type static_cast<std::underlying_type_t<enum_class>>(rhs); \
	}
