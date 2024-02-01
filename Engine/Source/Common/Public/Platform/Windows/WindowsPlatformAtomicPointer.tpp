/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#ifdef PLATFORM_WINDOWS
/**
 * For windows we use volatile
 */
template <TPointer T>
class TAtomicPointer
{
	using AtomicType = T volatile;
	using ValueType = T;

public:
	TAtomicPointer() = default;
	explicit TAtomicPointer(ValueType Other)
	{
		Ptr = Other;
	}
	TAtomicPointer& operator=(ValueType Other)
	{
		Ptr = Other;
		return *this;
	}
	TAtomicPointer(const TAtomicPointer& Other)
	{
		Ptr = (ValueType)Other;
	}
	TAtomicPointer& operator=(const TAtomicPointer& Other)
	{
		Ptr = (ValueType)Other;
		return *this;
	}
	~TAtomicPointer()
	{
		if (Ptr)
		{
			delete Ptr;
		}
	}
	/**
	 * Release inner pointer without deleting it!
	 */
	ValueType Release()
	{
		return (ValueType)InterlockedExchangePointer((void&&)Ptr, nullptr);
	}

	operator ValueType() const
	{
		return Ptr;
	}

	operator bool() const
	{
		return Ptr != nullptr;
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
	auto Destination = (void**)&obj->Ptr;
	auto Exchange = desired;
	return (T)InterlockedExchangePointer(Destination, Exchange);
}

template <TPointer T>
bool FGenericPlatformAtomicPointer::AtomicCompareExchange(TAtomicPointer<T>* obj, typename TAtomicPointer<T>::ValueType* expected,
	typename TAtomicPointer<T>::ValueType desired) noexcept
{
	auto Destination = (void**)&obj->Ptr;
	auto Exchange = desired;
	auto Comperand = *expected;
	return Comperand == (T)InterlockedCompareExchangePointer(Destination, Exchange, Comperand);
}

#endif