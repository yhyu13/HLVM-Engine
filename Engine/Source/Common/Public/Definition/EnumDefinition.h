/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include <magic_enum.hpp>

#define HLVM_ENUM(enum_class, type, ...) \
	enum class enum_class : type         \
	{                                    \
		__VA_ARGS__,                     \
		_NUM                             \
	};                                   \
	HLVM_INLINE_VAR constexpr size_t enum_class##_NUM = static_cast<size_t>(enum_class::_NUM)

#define HLVM_ENUM_UNDERLYING(enum_class) std::underlying_type_t<enum_class>
#define HLVM_ENUM_VALUE(enum_value) magic_enum::enum_underlying(enum_value)

#define HLVM_ENUM_TO_TCHAR(enum_value) TO_TCHAR_CSTR(magic_enum::enum_name((enum_value)).data())

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
		using EnumValue = HLVM_ENUM_UNDERLYING(enum_class);         \
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

template <class EnumType>
class TEnumAsUnderlying
{
	using EnumValue = HLVM_ENUM_UNDERLYING(EnumType);
	static_assert(std::is_enum_v<EnumType>, "TEnumAsUnderlying is not intended for use with enum classes");

public:
	TEnumAsUnderlying() = default;
	TEnumAsUnderlying(const TEnumAsUnderlying&) = default;
	TEnumAsUnderlying& operator=(const TEnumAsUnderlying&) = default;

	/**
	 * Constructor, initialize to the enum value.
	 *
	 * @param InEnum enum value to construct with.
	 */
	TEnumAsUnderlying(EnumType InEnum)
		: Value(S_C(EnumValue, InEnum))
	{
	}

	/**
	 * Constructor, initialize to the int32 value.
	 *
	 * @param InValue value to construct with.
	 */
	explicit TEnumAsUnderlying(TUINT32 InValue)
		: Value(static_cast<EnumValue>(InValue))
	{
	}

public:
	/**
	 * Compares two enumeration values for equality.
	 *
	 * @param InValue The value to compare with.
	 * @return true if the two values are equal, false otherwise.
	 */
	bool operator==(EnumType InValue) const
	{
		return static_cast<EnumType>(Value) == InValue;
	}

	/**
	 * Compares two enumeration values for equality.
	 *
	 * @param InValue The value to compare with.
	 * @return true if the two values are equal, false otherwise.
	 */
	bool operator==(TEnumAsUnderlying InValue) const
	{
		return Value == InValue.Value;
	}

	/** Implicit conversion to EnumType. */
	operator EnumType() const
	{
		return static_cast<EnumType>(Value);
	}

	/**
	 * Gets the enumeration value.
	 *
	 * @return The enumeration value.
	 */
	EnumType GetValue() const
	{
		return static_cast<EnumType>(Value);
	}

	/**
	 * Gets the integer enumeration value.
	 *
	 * @return The enumeration value.
	 */
	TUINT32 GetUIntValue() const
	{
		return S_C(TUINT32, Value);
	}

private:
	EnumValue Value;
};
