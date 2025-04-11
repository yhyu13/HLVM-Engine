/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include <magic_enum.hpp>

#define HLVM_ENUM(enum_class, type, ...) \
	enum class enum_class : type         \
	{                                    \
		__VA_ARGS__,                     \
		HLVM_NUM                         \
	};                                   \
	HLVM_INLINE_VAR constexpr size_t enum_class##_NUM = static_cast<size_t>(enum_class::HLVM_NUM)

#define HLVM_ENUM_UNDERLYING_T(enum_class) std::underlying_type_t<enum_class>
#define HLVM_ENUM_VALUE(enum_value) magic_enum::enum_underlying(enum_value)
#define HLVM_ENUM_VALUE_AS_TYPE(type, enum_value) static_cast<type>((enum_value))

#define HLVM_ENUM_VALUE_TO_TCHAR(enum_value) TO_TCHAR_CSTR(magic_enum::enum_name((enum_value)).data())

// Reference https://www.reddit.com/r/cpp/comments/13psi6f/comment/jleje26/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1&utm_content=share_button
#define HLVM_DECLARE_FLAGS_OPERATOR(Flags, op)                            \
	inline friend Flags operator op(Flags::EnumType a, Flags::EnumType b) \
	{                                                                     \
		return Flags((EnumValue(a) op EnumValue(b)));                     \
	}                                                                     \
	inline friend Flags operator op(Flags a, Flags::EnumType b)           \
	{                                                                     \
		return Flags((a.value op EnumValue(b)));                          \
	}                                                                     \
	inline friend Flags operator op(Flags::EnumType a, Flags b)           \
	{                                                                     \
		return Flags((EnumValue(a) op b.value));                          \
	}                                                                     \
	inline friend Flags operator op(Flags a, Flags b)                     \
	{                                                                     \
		return Flags((a.value op b.value));                               \
	}

#define HLVM_DECLARE_FLAGS_OPERATOR2(Flags, op)                    \
	inline friend Flags& operator op(Flags & a, Flags::EnumType b) \
	{                                                              \
		return a = Flags(a.value op EnumValue(b));                 \
	}                                                              \
	inline friend Flags& operator op(Flags & a, Flags b)           \
	{                                                              \
		return a = Flags(a.value op b.value);                      \
	}

#define HLVM_DECLARE_FLAGS_OPERATOR3(Flags)                   \
	inline friend bool operator==(Flags a, Flags::EnumType b) \
	{                                                         \
		return a.value == EnumValue(b);                       \
	}                                                         \
	inline friend bool operator==(Flags a, Flags b)           \
	{                                                         \
		return a.value == b.value;                            \
	}                                                         \
	inline friend bool operator!=(Flags a, Flags::EnumType b) \
	{                                                         \
		return a.value != EnumValue(b);                       \
	}                                                         \
	inline friend bool operator!=(Flags a, Flags b)           \
	{                                                         \
		return a.value != b.value;                            \
	}

#define HLVM_DECLARE_ENMU_FLAGS(enum_class, Flags)                  \
	struct Flags                                                    \
	{                                                               \
		using EnumType = enum_class;                                \
		using EnumValue = HLVM_ENUM_UNDERLYING_T(enum_class);       \
		EnumValue value{ 0 };                                       \
		inline constexpr Flags() {}                                 \
		inline constexpr Flags(EnumType v) : value(EnumValue(v)) {} \
		inline constexpr Flags(EnumValue v) : value(v) {}           \
		inline constexpr operator EnumValue() const                 \
		{                                                           \
			return value;                                           \
		}                                                           \
		HLVM_DECLARE_FLAGS_OPERATOR(Flags, &)                       \
		HLVM_DECLARE_FLAGS_OPERATOR2(Flags, &=)                     \
		HLVM_DECLARE_FLAGS_OPERATOR(Flags, |)                       \
		HLVM_DECLARE_FLAGS_OPERATOR2(Flags, |=)                     \
		HLVM_DECLARE_FLAGS_OPERATOR3(Flags)                         \
	};
