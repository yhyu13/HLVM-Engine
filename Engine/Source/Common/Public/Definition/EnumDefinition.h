/**
 * Copyright (c) 2026. MIT License. All rights reserved.
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

// magic enum don't enum class underlying type
#define HLVM_ENUM_TO_UNDERLYING(enum_class) std::underlying_type_t<enum_class>
#define E2UNDERLYING(enum_class) HLVM_ENUM_TO_UNDERLYING(enum_class)

#define HLVM_ENUM_TO_VALUE(enum_value) magic_enum::enum_underlying(enum_value)
#define E2VALUE(enum_value) HLVM_ENUM_TO_VALUE(enum_value)

#define HLVM_ENUM_TO_TCHAR(enum_value) TO_TCHAR_CSTR(magic_enum::enum_name((enum_value)).data())
#define E2TCHAR(enum_value) HLVM_ENUM_TO_TCHAR(enum_value)

// Reference https://www.reddit.com/r/cpp/comments/13psi6f/comment/jleje26/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1&utm_content=share_button
#define _HLVM_DECLARE_FLAGS_OPERATOR(enum_class, Flags, op)          \
	inline Flags operator op(enum_class a, enum_class b)            \
	{                                                               \
		return Flags((Flags::EnumValue(a) op Flags::EnumValue(b))); \
	}                                                               \
	inline Flags operator op(Flags a, enum_class b)                 \
	{                                                               \
		return Flags((a.value op Flags::EnumValue(b)));             \
	}                                                               \
	inline Flags operator op(enum_class a, Flags b)                 \
	{                                                               \
		return Flags((Flags::EnumValue(a) op b.value));             \
	}                                                               \
	inline Flags operator op(Flags a, Flags b)                      \
	{                                                               \
		return Flags((a.value op b.value));                         \
	}

#define _HLVM_DECLARE_FLAGS_OPERATOR2(enum_class, Flags, op) \
	inline Flags& operator op(Flags & a, enum_class b)      \
	{                                                       \
		return a = Flags(a.value op Flags::EnumValue(b));   \
	}                                                       \
	inline Flags& operator op(Flags & a, Flags b)           \
	{                                                       \
		return a = Flags(a.value op b.value);               \
	}

#define _HLVM_DECLARE_FLAGS_OPERATOR3(enum_class, Flags) \
	inline bool operator==(Flags a, enum_class b)       \
	{                                                   \
		return a.value == Flags::EnumValue(b);          \
	}                                                   \
	inline bool operator==(Flags a, Flags b)            \
	{                                                   \
		return a.value == b.value;                      \
	}                                                   \
	inline bool operator!=(Flags a, enum_class b)       \
	{                                                   \
		return a.value != Flags::EnumValue(b);          \
	}                                                   \
	inline bool operator!=(Flags a, Flags b)            \
	{                                                   \
		return a.value != b.value;                      \
	}

#define HLVM_ENMU_CLASS_FLAGS(enum_class, Flags)                  \
	struct Flags                                                    \
	{                                                               \
		using EnumType = enum_class;                                \
		using EnumValue = HLVM_ENUM_TO_UNDERLYING(enum_class);         \
		EnumValue value{ 0 };                                       \
		inline constexpr Flags() {}                                 \
		inline constexpr Flags(EnumType v) : value(EnumValue(v)) {} \
		inline constexpr Flags(EnumValue v) : value(v) {}           \
		inline constexpr operator EnumValue() const                 \
		{                                                           \
			return value;                                           \
		}                                                           \
	};                                                              \
	_HLVM_DECLARE_FLAGS_OPERATOR(enum_class, Flags, &)               \
	_HLVM_DECLARE_FLAGS_OPERATOR2(enum_class, Flags, &=)             \
	_HLVM_DECLARE_FLAGS_OPERATOR(enum_class, Flags, |)               \
	_HLVM_DECLARE_FLAGS_OPERATOR2(enum_class, Flags, |=)             \
	_HLVM_DECLARE_FLAGS_OPERATOR3(enum_class, Flags)

template <typename EnumFlags>
bool EnumHasAnyFlags(EnumFlags&& GivenFlags, EnumFlags&& FlagsToCheck)
{
	return (GivenFlags & FlagsToCheck);
}

template <typename EnumFlags, typename EnumType>
bool EnumHasAnyFlags(EnumFlags&& GivenFlags, EnumType&& FlagsToCheck)
{
	return (GivenFlags & FlagsToCheck);
}

template <typename EnumFlags>
bool EnumHasAllFlags(EnumFlags&& GivenFlags, EnumFlags&& FlagsToCheck)
{
	return (GivenFlags & FlagsToCheck) == GivenFlags;
}

template <typename EnumFlags, typename EnumType>
bool EnumHasAllFlags(EnumFlags&& GivenFlags, EnumType&& FlagsToCheck)
{
	return (GivenFlags & FlagsToCheck) == GivenFlags;
}

template <class EnumType>
class TEnumAsUnderlying
{
	using EnumValue = HLVM_ENUM_TO_UNDERLYING(EnumType);
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
