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
 *  @details On construction, the TRefObject increment the reference counting.
 *          On destruction, the TRefObject tries to decrement the reference counting.
 */
template <ReferenceCountable T>
class TRefObject
{
public:
	// Big Five----------------------------------------------------------------------------------------------------
	TRefObject() = default;

	explicit TRefObject(T* obj)
		: m_ptr(obj)
	{
		m_ptr->IncrementRef();
	}

	explicit TRefObject(const T& obj)
		requires(std::is_copy_constructible_v<T>)
		: m_ptr(new T(CopyTemp(obj)))
	{
		m_ptr->IncrementRef();
	}

	explicit TRefObject(T&& obj)
		requires(std::is_move_constructible_v<T>)
		: m_ptr(new T(MoveTemp(obj)))
	{
		m_ptr->IncrementRef();
	}

	~TRefObject()
	{
		if (m_ptr && !m_ptr->DecrementRef())
		{
			delete m_ptr;
		}
		m_ptr = nullptr;
	}

	TRefObject(const TRefObject& other)
		: m_ptr(other.m_ptr)
	{
		if (m_ptr)
		{
			m_ptr->IncrementRef();
		}
	}

	TRefObject(TRefObject&& other) noexcept
		: m_ptr(other.m_ptr)
	{
		other.m_ptr = nullptr;
	}

	TRefObject& operator=(const TRefObject& other)
	{
		if (this != &other)
		{
			this->~TRefObject();
			m_ptr = other.m_ptr;
			if (m_ptr)
			{
				m_ptr->IncrementRef();
			}
		}
		return *this;
	}

	TRefObject& operator=(TRefObject&& other) noexcept
	{
		if (this != &other)
		{
			this->~TRefObject();
			m_ptr = other.m_ptr;
			other.m_ptr = nullptr;
		}
		return *this;
	}

	// Polymorphism Casting---------------------------------------------------------------------------------------------

	template <class U>
	TRefObject(const TRefObject<U>&) noexcept
	{
		static_assert(std::is_same_v<T, U>, "TRefObject Polymorphism Casting not allowed!");
	}

	template <class U>
	TRefObject(TRefObject<U>&&) noexcept
	{
		static_assert(std::is_same_v<T, U>, "TRefObject Polymorphism Casting not allowed!");
	}

	template <class U>
	TRefObject& operator=(const TRefObject<U>&) noexcept
	{
		static_assert(std::is_same_v<T, U>, "TRefObject Polymorphism Casting not allowed!");
	}

	template <class U>
	TRefObject& operator=(TRefObject<U>&&) noexcept
	{
		static_assert(std::is_same_v<T, U>, "TRefObject Polymorphism Casting not allowed!");
	}

	// Operator overloading---------------------------------------------------------------------------------------------

	T* operator->() noexcept
	{
		auto bValid = Valid();
		HLVM_ASSERT(bValid, TXT("TRefObject nullptr error!"));
		return m_ptr;
	}

	T& operator*() noexcept
	{
		auto bValid = Valid();
		HLVM_ASSERT(bValid, TXT("TRefObject nullptr error!"));
		return *m_ptr;
	}

	const T* operator->() const noexcept
	{
		auto bValid = Valid();
		HLVM_ASSERT(bValid, TXT("TRefObject nullptr error!"));
		return m_ptr;
	}

	const T& operator*() const noexcept
	{
		auto bValid = Valid();
		HLVM_ASSERT(bValid, TXT("TRefObject nullptr error!"));
		return *m_ptr;
	}

	// Methods---------------------------------------------------------------------------------------------

	template <bool bValidate = !HLVM_BUILD_RELEASE>
	HLVM_NODISCARD T* Get() noexcept
	{
		if constexpr (bValidate)
		{
			HLVM_ENSURE(Valid(), TXT("TRefObject nullptr error!"));
		}
		return m_ptr;
	}

	template <bool bValidate = !HLVM_BUILD_RELEASE>
	HLVM_NODISCARD const T* Get() const noexcept
	{
		if constexpr (bValidate)
		{
			HLVM_ENSURE(Valid(), TXT("TRefObject nullptr error!"));
		}
		return m_ptr;
	}

	void Reset() noexcept
	{
		this->~TRefObject();
	}

	HLVM_NODISCARD bool Valid() const noexcept
	{
		return m_ptr != nullptr;
	}

private:
	T* m_ptr{ nullptr };
};

/*
	Custum hash function for TRefObject<T>
*/
namespace std
{
	template <class T>
	struct hash<TRefObject<T>>
	{
		std::size_t operator()(const TRefObject<T>& ptr) const noexcept
		{
			return hash<T*>()(ptr.template Get<false>());
		}
	};
} // namespace std
