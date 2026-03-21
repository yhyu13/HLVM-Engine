/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include <type_traits>

template <typename T>
struct TConstRemoved
{
	using Type = T;
};

template <typename T>
struct TConstRemoved<const T&>
{
	using Type = T;
};

template <typename T>
struct TReferenceRemoved
{
	using Type = T;
};

template <typename T>
struct TReferenceRemoved<T&>
{
	using Type = T;
};

template <typename T>
struct TReferenceRemoved<T&&>
{
	using Type = T;
};

template <typename T>
HLVM_NODISCARD constexpr T&& FwdTemp(typename TReferenceRemoved<T>::Type& Var) noexcept
{
	return static_cast<T&&>(Var);
}

template <typename T>
HLVM_NODISCARD constexpr typename TReferenceRemoved<T>::Type&& MoveTemp(T&& Var) noexcept
{
	using OutType = typename TReferenceRemoved<T>::Type;
	static_assert(!std::is_same_v<const OutType&, OutType&>, "Move should not be used on const object");
	return static_cast<OutType&&>(Var);
}

template <typename T>
HLVM_NODISCARD constexpr typename TReferenceRemoved<T>::Type CopyTemp(T& Var) noexcept
{
	using OutType = typename TReferenceRemoved<T>::Type;
	return const_cast<const OutType&>(Var);
}

template <typename T>
HLVM_NODISCARD constexpr typename TReferenceRemoved<T>::Type CopyTemp(const T& Var) noexcept
{
	using OutType = typename TReferenceRemoved<T>::Type;
	return const_cast<const OutType&>(Var);
}

template<typename T>
HLVM_NODISCARD constexpr T&& ForwardTemp(TReferenceRemoved<T>& t) {
	return static_cast<T&&>(t);
}
