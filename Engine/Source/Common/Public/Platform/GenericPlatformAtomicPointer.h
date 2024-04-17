/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "GenericPlatform.h"

#include <concepts>

// Concept https://en.cppreference.com/w/cpp/language/constraints
template <class T>
concept TPointer = std::is_pointer<T>::value;

template <TPointer T>
class TAtomicPointer;

class FGenericPlatformAtomicPointer
{
public:
	/**
	 * Mimic std::atomic_exchange
	 * @tparam T Must be Pointer type
	 * @param obj the object to exchange to a new value
	 * @param desired the value to be exchanged
	 * @return old obj value
	 */
	template <TPointer T>
	static T AtomicExchange(TAtomicPointer<T>* obj, typename TAtomicPointer<T>::ValueType desired) noexcept;

	/**
	 * Mimic std::atomic_compare_exchange
	 * @tparam T Must be Pointer type
	 * @param obj the object to exchange to a new value
	 * @param expected the value to be compared, if it is equal to obj, then exchange obj to desired
	 * @param desired the value to be exchanged
	 * @return true if exchange was successful
	 */
	template <TPointer T>
	static bool AtomicCompareExchange(TAtomicPointer<T>* obj, typename TAtomicPointer<T>::ValueType* expected,
		typename TAtomicPointer<T>::ValueType desired) noexcept;
};

#include "LinuxGNU/LinuxGNUPlatformAtomicPointer.tpp"
#include "Windows/WindowsPlatformAtomicPointer.tpp"
