/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#ifdef PLATFORM_LINUXGNU

	#include <atomic>
	#include <stdatomic.h>

/**
 * For linux we use std::atomic
 */
template <TPointer T>
class TAtomicPointer
{
	using AtomicType = std::atomic<T>;
	using ValueType = T;
	static_assert(sizeof(ValueType) == sizeof(AtomicType), "TAtomicPointer: ValueType and AtomicType must have the same size!");

public:
	TAtomicPointer() = default;
	explicit TAtomicPointer(ValueType Other) noexcept
	{
		Ptr.store(Other, std::memory_order_release);
	}
	TAtomicPointer& operator=(ValueType Other) noexcept
	{
		Ptr.store(Other, std::memory_order_release);
		return *this;
	}
	TAtomicPointer(const TAtomicPointer& Other) noexcept
	{
		Ptr.store(static_cast<ValueType>(Other), std::memory_order_release);
	}
	TAtomicPointer& operator=(const TAtomicPointer& Other) noexcept
	{
		if (this != &Other)
		{
			Ptr.store(static_cast<ValueType>(Other), std::memory_order_release);
		}
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
	bool IsLockFree() const noexcept
	{
		return Ptr.is_always_lock_free;
	}

	operator ValueType() const noexcept
	{
		return Ptr.load(std::memory_order_acquire);
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

	friend typename TPointerRemoved<T>::Type& operator*(const TAtomicPointer& self)
	{
		return *(static_cast<ValueType>(self));
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
