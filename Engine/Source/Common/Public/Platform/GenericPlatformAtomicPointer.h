/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "GenericPlatform.h"

#if !defined(PLATFORM_WINDOWS)
	#include <atomic>
#endif
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
	 * @tparam T
	 * @param obj
	 * @param desired
	 * @return old obj value
	 */
	template <TPointer T>
	static T AtomicExchange(TAtomicPointer<T>* obj, typename TAtomicPointer<T>::ValueType desired) noexcept;

	/**
	 * Mimic std::atomic_compare_exchange
	 * @tparam T
	 * @param obj
	 * @param expected
	 * @param desired
	 * @return true if exchange was successful
	 */
	template <TPointer T>
	static bool AtomicCompareExchange(TAtomicPointer<T>* obj, typename TAtomicPointer<T>::ValueType* expected,
		typename TAtomicPointer<T>::ValueType desired) noexcept;
};

#include "LinuGNU/LinuxGNUPlatformAtomicPointer.tpp"
#include "Windows/WindowsPlatformAtomicPointer.tpp"