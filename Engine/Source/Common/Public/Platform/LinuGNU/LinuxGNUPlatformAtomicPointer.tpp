/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#ifdef PLATFORM_LINUXGNU
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
	explicit TAtomicPointer(ValueType Other)
	{
		Ptr.store(Other, std::memory_order_acquire);
	}
	TAtomicPointer& operator=(ValueType Other)
	{
		Ptr.store(Other, std::memory_order_acquire);
		return *this;
	}
	TAtomicPointer(const TAtomicPointer& Other)
	{
		Ptr.store((ValueType)Other, std::memory_order_acquire);
	}
	TAtomicPointer& operator=(const TAtomicPointer& Other)
	{
		Ptr.store((ValueType)Other, std::memory_order_acquire);
		return *this;
	}
	~TAtomicPointer()
	{
		if (ValueType ptr = (ValueType)Ptr)
		{
			delete ptr;
		}
	}
	/**
	 * Release inner pointer without deleting it!
	 */
	ValueType Release()
	{
		return Ptr.exchange(nullptr, std::memory_order_relaxed);
	}

	operator ValueType() const
	{
		return Ptr.load(std::memory_order_release);
	}

	operator bool() const
	{
		return (ValueType)Ptr != nullptr;
	}

	bool operator==(const ValueType& Other) const
	{
		return (ValueType)(*this) == Other;
	}

	bool operator!=(const ValueType& Other) const
	{
		return !((*this) == Other);
	}

	ValueType volatile& operator->()
	{
		return (ValueType volatile&)Ptr;
	}

	const ValueType volatile& operator->() const
	{
		return (const ValueType volatile&)Ptr;
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
	return std::atomic_compare_exchange_strong_explicit(&obj->Ptr, expected, desired, std::memory_order_acquire, std::memory_order_release);
}

#endif