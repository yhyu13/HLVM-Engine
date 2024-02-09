/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "ReferenceTemplate.tpp"

#include <type_traits>

template <typename T>
inline typename TReferenceRemoved<T>::Type&& MoveTemp(T&& Var)
{
	using OutType = typename TReferenceRemoved<T>::Type;
	static_assert(!std::is_same_v<const OutType&, OutType&>, "Move should not be used on const object");
	return (OutType&&)Var;
}

template <typename T>
inline typename TReferenceRemoved<T>::Type CopyTemp(T& Var)
{
	using OutType = typename TReferenceRemoved<T>::Type;
	return const_cast<const OutType&>(Var);
}

template <typename T>
inline typename TReferenceRemoved<T>::Type CopyTemp(const T& Var)
{
	using OutType = typename TReferenceRemoved<T>::Type;
	return const_cast<const OutType&>(Var);
}