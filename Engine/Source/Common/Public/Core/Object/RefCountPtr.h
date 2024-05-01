/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "RefCountable.h"

/**
 *  @brief A reference pointer requires template class instance method (1) Increment reference (2) Decrement reference
 *  The template class should maintain a mutable atomic reference counter, and the template class should align itself to avoid
 *  false sharing.
 *
 *  @details On construction, the TRefCountPtr increment the reference counting.
 *          On destruction, the TRefCountPtr tries to decrement the reference counting.
 */
template <CRefCountable T>
class TRefCountPtr
{
public:
	// Big Five----------------------------------------------------------------------------------------------------
	TRefCountPtr() = default;

	explicit TRefCountPtr(T* obj)
		: m_ptr(obj)
	{
		m_ptr->IncrementRef();
	}

	~TRefCountPtr()
	{
		if (m_ptr && !m_ptr->DecrementRef())
		{
			delete m_ptr;
		}
		m_ptr = nullptr;
	}

	TRefCountPtr(const TRefCountPtr& other)
		: m_ptr(other.m_ptr)
	{
		if (m_ptr)
		{
			m_ptr->IncrementRef();
		}
	}

	TRefCountPtr(TRefCountPtr&& other) noexcept
		: m_ptr(other.m_ptr)
	{
		other.m_ptr = nullptr;
	}

	TRefCountPtr& operator=(const TRefCountPtr& other)
	{
		if (this != &other)
		{
			this->~TRefCountPtr();
			m_ptr = other.m_ptr;
			if (m_ptr)
			{
				m_ptr->IncrementRef();
			}
		}
		return *this;
	}

	TRefCountPtr& operator=(TRefCountPtr&& other) noexcept
	{
		if (this != &other)
		{
			this->~TRefCountPtr();
			m_ptr = other.m_ptr;
			other.m_ptr = nullptr;
		}
		return *this;
	}

	// Polymorphism Casting---------------------------------------------------------------------------------------------

	template <class U>
	TRefCountPtr(const TRefCountPtr<U>&) noexcept
	{
		static_assert(std::is_same_v<T, U>, "TRefCountPtr Polymorphism Casting not allowed!");
	}

	template <class U>
	TRefCountPtr(TRefCountPtr<U>&&) noexcept
	{
		static_assert(std::is_same_v<T, U>, "TRefCountPtr Polymorphism Casting not allowed!");
	}

	template <class U>
	TRefCountPtr& operator=(const TRefCountPtr<U>&) noexcept
	{
		static_assert(std::is_same_v<T, U>, "TRefCountPtr Polymorphism Casting not allowed!");
	}

	template <class U>
	TRefCountPtr& operator=(TRefCountPtr<U>&&) noexcept
	{
		static_assert(std::is_same_v<T, U>, "TRefCountPtr Polymorphism Casting not allowed!");
	}

	// Operator overloading---------------------------------------------------------------------------------------------

	T* operator->() noexcept
	{
		auto bValid = Valid();
		HLVM_ASSERT(bValid, TXT("TRefCountPtr nullptr error!"));
		return m_ptr;
	}

	T& operator*() noexcept
	{
		auto bValid = Valid();
		HLVM_ASSERT(bValid, TXT("TRefCountPtr nullptr error!"));
		return *m_ptr;
	}

	const T* operator->() const noexcept
	{
		auto bValid = Valid();
		HLVM_ASSERT(bValid, TXT("TRefCountPtr nullptr error!"));
		return m_ptr;
	}

	const T& operator*() const noexcept
	{
		auto bValid = Valid();
		HLVM_ASSERT(bValid, TXT("TRefCountPtr nullptr error!"));
		return *m_ptr;
	}

	// Methods---------------------------------------------------------------------------------------------

	template <bool bValidate = !HLVM_BUILD_RELEASE>
	HLVM_NODISCARD T* Get() noexcept
	{
		if constexpr (bValidate)
		{
			HLVM_ENSURE(Valid(), TXT("TRefCountPtr nullptr error!"));
		}
		return m_ptr;
	}

	template <bool bValidate = !HLVM_BUILD_RELEASE>
	HLVM_NODISCARD const T* Get() const noexcept
	{
		if constexpr (bValidate)
		{
			HLVM_ENSURE(Valid(), TXT("TRefCountPtr nullptr error!"));
		}
		return m_ptr;
	}

	void Reset() noexcept
	{
		this->~TRefCountPtr();
	}

	HLVM_NODISCARD bool Valid() const noexcept
	{
		return m_ptr != nullptr;
	}

private:
	T* m_ptr{ nullptr };
};

/*
	Custum hash function for TRefCountPtr<T>
*/
namespace std
{
	template <class T>
	struct hash<TRefCountPtr<T>>
	{
		std::size_t operator()(const TRefCountPtr<T>& ptr) const noexcept
		{
			return hash<T*>()(ptr.template Get<false>());
		}
	};
} // namespace std
