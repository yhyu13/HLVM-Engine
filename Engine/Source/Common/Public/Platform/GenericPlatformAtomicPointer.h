/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "GenericPlatform.h"
#include "Template/ConceptTemplate.tpp"

template <CPointer T>
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
	template <CPointer T>
	static T AtomicExchange(TAtomicPointer<T>* obj, typename TAtomicPointer<T>::ValueType desired) noexcept;

	/**
	 * Mimic std::atomic_compare_exchange
	 * @tparam T Must be Pointer type
	 * @param obj the object to exchange to a new value
	 * @param expected the value to be compared, if it is equal to obj, then exchange obj to desired
	 * @param desired the value to be exchanged
	 * @return true if exchange was successful
	 */
	template <CPointer T>
	static bool AtomicCompareExchange(TAtomicPointer<T>* obj, typename TAtomicPointer<T>::ValueType* expected,
		typename TAtomicPointer<T>::ValueType desired) noexcept;
};

#include "LinuxGNU/LinuxGNUPlatformAtomicPointer.tpp"
#include "Windows/WindowsPlatformAtomicPointer.tpp"
