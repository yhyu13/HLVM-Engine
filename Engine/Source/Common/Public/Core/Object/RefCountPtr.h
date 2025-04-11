/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "RefCountable.h"
#include "Core/Assert.h"

/**
 *  @brief A reference pointer requires template class instance method (1) Increment reference (2) Decrement reference
 *  The template class should maintain a mutable atomic reference counter, and the template class should align itself to avoid
 *  false sharing.
 *
 *  @details On construction, the TRefCountPtr increment the reference counting.
 *          On destruction, the TRefCountPtr tries to decrement the reference counting.
 */
template <CRefCountable T, bool bAllowPolymorphic = true>
class TRefCountPtr
{
public:
	// Big Five----------------------------------------------------------------------------------------------------
	TRefCountPtr() = default;

	TRefCountPtr(T* obj)
		: m_ptr(obj)
	{
		if (m_ptr)
		{
			m_ptr->IncrementRef();
		}
	}

	template <class U>
	TRefCountPtr(U* obj)
	{
		if constexpr (!bAllowPolymorphic)
		{
			static_assert(std::is_same_v<T, U>, "TRefCountPtr Polymorphism Casting not allowed!");
		}
		else
		{
			m_ptr = static_cast<T*>(obj);
			if (m_ptr)
			{
				m_ptr->IncrementRef();
			}
		}
	}

	~TRefCountPtr()
	{
		Reset();
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
			Reset(); // Reset first
			m_ptr = other.m_ptr; // Copy
			if (m_ptr)
			{
				m_ptr->IncrementRef(); // Increment
			}
		}
		return *this;
	}

	TRefCountPtr& operator=(TRefCountPtr&& other) noexcept
	{
		if (this != &other)
		{
			Reset(); // Destructor first
			m_ptr = other.m_ptr; // Copy
			other.m_ptr = nullptr; // Reset
		}
		return *this;
	}

	// Polymorphism Casting---------------------------------------------------------------------------------------------

	template <class U>
	TRefCountPtr(const TRefCountPtr<U>& other) noexcept
	{
		if constexpr (!bAllowPolymorphic)
		{
			static_assert(std::is_same_v<T, U>, "TRefCountPtr Polymorphism Casting not allowed!");
		}
		else
		{
			m_ptr = static_cast<T*>(other.template Get<false>());
			if (m_ptr)
			{
				m_ptr->IncrementRef();
			}
		}
	}

	template <class U>
	TRefCountPtr(TRefCountPtr<U>&& other) noexcept
	{
		if constexpr (!bAllowPolymorphic)
		{
			static_assert(std::is_same_v<T, U>, "TRefCountPtr Polymorphism Casting not allowed!");
		}
		else
		{
			m_ptr = static_cast<T*>(other.template Get<false>());
			// In principle, we no need to change ref.
			// But since we cannnot access U's m_ptr member,
			// we have to increment ref in here and then decrement in other by Reset()
			if (m_ptr)
			{
				m_ptr->IncrementRef();
			}
			other.Reset();
		}
	}

	template <class U>
	TRefCountPtr& operator=(const TRefCountPtr<U>& other) noexcept
	{
		if constexpr (!bAllowPolymorphic)
		{
			static_assert(std::is_same_v<T, U>, "TRefCountPtr Polymorphism Casting not allowed!");
		}
		else
		{
			Reset();
			m_ptr = static_cast<T*>(other.template Get<false>());
			if (m_ptr)
			{
				m_ptr->IncrementRef();
			}
		}
	}

	template <class U>
	TRefCountPtr& operator=(TRefCountPtr<U>&& other) noexcept
	{
		if constexpr (!bAllowPolymorphic)
		{
			static_assert(std::is_same_v<T, U>, "TRefCountPtr Polymorphism Casting not allowed!");
		}
		else
		{
			Reset();
			m_ptr = static_cast<T*>(other.template Get<false>());
			// In principle, we no need to change ref.
			// But since we cannnot access U's m_ptr member,
			// we have to increment ref in here and then decrement in other by Reset()
			if (m_ptr)
			{
				m_ptr->IncrementRef();
			}
			other.Reset();
		}
	}

	// Operator overloading---------------------------------------------------------------------------------------------

	T* operator->() noexcept
	{
		if constexpr (!HLVM_BUILD_RELEASE)
		{
			HLVM_ENSURE_F(Valid(), TXT("TRefCountPtr nullptr error!"));
		}
		return m_ptr;
	}

	T& operator*() noexcept
	{
		if constexpr (!HLVM_BUILD_RELEASE)
		{
			HLVM_ENSURE_F(Valid(), TXT("TRefCountPtr nullptr error!"));
		}
		return *m_ptr;
	}

	const T* operator->() const noexcept
	{
		if constexpr (!HLVM_BUILD_RELEASE)
		{
			HLVM_ENSURE_F(Valid(), TXT("TRefCountPtr nullptr error!"));
		}
		return m_ptr;
	}

	const T& operator*() const noexcept
	{
		if constexpr (!HLVM_BUILD_RELEASE)
		{
			HLVM_ENSURE_F(Valid(), TXT("TRefCountPtr nullptr error!"));
		}
		return *m_ptr;
	}

	// Methods---------------------------------------------------------------------------------------------

	template <bool bValidate = !HLVM_BUILD_RELEASE>
	HLVM_NODISCARD T* Get() noexcept
	{
		return m_ptr;
	}

	template <bool bValidate = !HLVM_BUILD_RELEASE>
	HLVM_NODISCARD T* Get() const noexcept
	{
		return m_ptr;
	}

	void Reset() noexcept
	{
		if (m_ptr && !m_ptr->DecrementRef())
		{
			delete m_ptr;
		}
		m_ptr = nullptr;
	}

	HLVM_NODISCARD bool Valid() const noexcept
	{
		return m_ptr != nullptr;
	}

	HLVM_NODISCARD operator bool() const noexcept
	{
		return Valid();
	}

	// compare opeartor
	template <class U>
	HLVM_NODISCARD bool operator==(const TRefCountPtr<U>& other) const noexcept
	{
		static_assert(std::is_same_v<T, U>, "TRefCountPtr Polymorphism Casting not allowed!");
		return m_ptr == other.template Get<false>();
	}
	template <class U>
	HLVM_NODISCARD bool operator!=(const TRefCountPtr<U>& other) const noexcept
	{
		static_assert(std::is_same_v<T, U>, "TRefCountPtr Polymorphism Casting not allowed!");
		return m_ptr != other.template Get<false>();
	}

	// compare with nullptr
	HLVM_NODISCARD bool operator==(std::nullptr_t) const noexcept
	{
		return m_ptr == nullptr;
	}
	HLVM_NODISCARD bool operator!=(std::nullptr_t) const noexcept
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
