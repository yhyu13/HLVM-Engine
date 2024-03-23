/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#ifdef PLATFORM_LINUXGNU

	#include <atomic>
/**
 * For linux we use std::atomic
 */
template <TPointer T>
class TAtomicPointer
{
	using AtomicType = std::atomic<T>;
	using ValueType = T;

public:
	TAtomicPointer() = default;
	explicit TAtomicPointer(ValueType Other) noexcept
	{
		Ptr.store(Other, std::memory_order_acquire);
	}
	TAtomicPointer& operator=(ValueType Other) noexcept
	{
		Ptr.store(Other, std::memory_order_acquire);
		return *this;
	}
	TAtomicPointer(const TAtomicPointer& Other) noexcept
	{
		Ptr.store(static_cast<ValueType>(Other), std::memory_order_acquire);
	}
	TAtomicPointer& operator=(const TAtomicPointer& Other) noexcept
	{
		Ptr.store(static_cast<ValueType>(Other), std::memory_order_acquire);
		return *this;
	}
	~TAtomicPointer() noexcept
	{
		Release();
	}
	/**
	 * Release inner pointer without deleting it!
	 */
	ValueType Release() noexcept
	{
		return Ptr.exchange(nullptr, std::memory_order_relaxed);
	}

	operator ValueType() const noexcept
	{
		return Ptr.load(std::memory_order_release);
	}

	operator bool() const noexcept
	{
		return static_cast<ValueType>(Ptr) != nullptr;
	}

	bool operator==(const ValueType& Other) const noexcept
	{
		return static_cast<ValueType>(*this) == Other;
	}

	bool operator!=(const ValueType& Other) const noexcept
	{
		return !((*this) == Other);
	}

	ValueType operator->() noexcept
	{
		return static_cast<ValueType>(Ptr);
	}

	const ValueType operator->() const noexcept
	{
		return static_cast<ValueType>(Ptr);
	}

	friend FGenericPlatformAtomicPointer;

private:
	AtomicType Ptr;
};

template <TPointer T>
T FGenericPlatformAtomicPointer::AtomicExchange(TAtomicPointer<T>* obj, typename TAtomicPointer<T>::ValueType desired) noexcept
{
	return std::atomic_exchange_explicit(&obj->Ptr, desired, std::memory_order_acquire);
}

template <TPointer T>
bool FGenericPlatformAtomicPointer::AtomicCompareExchange(TAtomicPointer<T>* obj, typename TAtomicPointer<T>::ValueType* expected,
	typename TAtomicPointer<T>::ValueType desired) noexcept
{
	return std::atomic_compare_exchange_strong_explicit(&obj->Ptr, expected, desired, std::memory_order_acq_rel, std::memory_order_release);
}

#endif
