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
	explicit TAtomicPointer(ValueType Other) noexcept
	{
		Ptr = Other;
	}
	TAtomicPointer& operator=(ValueType Other) noexcept
	{
		Ptr = Other;
		return *this;
	}
	TAtomicPointer(const TAtomicPointer& Other) noexcept
	{
		Ptr = (ValueType)Other;
	}
	TAtomicPointer& operator=(const TAtomicPointer& Other) noexcept
	{
		if (this != &Other)
		{
			Ptr = (ValueType)Other;
		}
		return *this;
	}
	~TAtomicPointer() noexcept
	{
	}
	/**
	 * Release inner pointer without deleting it!
	 */
	ValueType Release() noexcept
	{
		return (ValueType)InterlockedExchangePointer((void&&)Ptr, nullptr);
	}
	bool IsLockFree() const noexcept
	{
		return true;
	}

	operator ValueType() const noexcept
	{
		return Ptr;
	}

	operator bool() const noexcept
	{
		return Ptr != nullptr;
	}

	bool operator==(const ValueType& Other) const noexcept
	{
		return (ValueType)(*this) == Other;
	}

	bool operator!=(const ValueType& Other) const noexcept
	{
		return !((*this) == Other);
	}

	ValueType operator->() noexcept
	{
		return (ValueType)Ptr;
	}

	const ValueType operator->() const noexcept
	{
		return (const ValueType)Ptr;
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
