/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

// Function to check if two pointers overlap
inline bool IsPointerOverlap(const void* ptr1, size_t size1, const void* ptr2)
{
	const char* cptr1 = reinterpret_cast<const char*>(ptr1);
	const char* cptr2 = reinterpret_cast<const char*>(ptr2);

	// Calculate the end pointer of ptr1 by adding its size
	const char* end_ptr1 = cptr1 + size1;

	// If ptr1 is smaller than or equal to ptr2, and end_ptr1 is greater than ptr2,
	// then there is an overlap
	return (cptr1 <= cptr2 && end_ptr1 > cptr2);
}

// To compare with another region, you can modify the function to take a second size:
inline bool IsPointerOverlap(const void* ptr1, size_t size1, const void* ptr2, size_t size2)
{
	const char* cptr1 = reinterpret_cast<const char*>(ptr1);
	const char* cptr2 = reinterpret_cast<const char*>(ptr2);

	// Calculate the end pointers for both regions
	const char* end_ptr1 = cptr1 + size1;
	const char* end_ptr2 = cptr2 + size2;

	// Check if any part of region 1 overlaps with region 2 or vice versa
	return (cptr1 <= cptr2 && end_ptr1 > cptr2)
		|| (cptr1 <= end_ptr2 && end_ptr1 > end_ptr2)
		|| (cptr2 <= cptr1 && end_ptr2 > cptr1)
		|| (cptr2 <= end_ptr1 && end_ptr2 > end_ptr1);
}

template <typename T>
struct TPointerRemoved
{
	using Type = T;
};

template <typename T>
struct TPointerRemoved<T*>
{
	using Type = T;
};

template <typename T>
struct TPointerRemoved<T**>
{
	using Type = T;
};

#include "Platform/PlatformDefinition.h"

/**
 * Use int32 offset to this pointer to represent another pointer
 * in order to achieve 4 bytes wide pointer approximately 1%~3% slower than using raw pointer
 * @CAUTION we assume that TOffsetPtr32 represent pointer range strictly smaller than 2GB
 */
template <typename T>
PACK(struct TOffsetPtr32 {
	// Initial value must be Largest positive integer int32 can represent
	// We will use this number to represent nullptr
	int32_t offset{ 0x7FFFFFFF };
	operator T*()
	{
		return (offset != 0x7FFFFFFF) ? R_C(T*, R_C(TBYTE*, this) + offset) : nullptr;
	}
	operator const T*() const
	{
		return (offset != 0x7FFFFFFF) ? R_C(const T*, R_C(const TBYTE*, this) + offset) : nullptr;
	}
	const T* operator=(const TOffsetPtr32& _rhs)
	{
		const T* rhs = _rhs;
		if (this != &_rhs)
		{
			(rhs != nullptr) ? offset = S_C(int32_t, (R_C(const TBYTE*, rhs) - R_C(TBYTE*, this))) : offset = 0x7FFFFFFF;
		}
		return rhs;
	}
	T* operator=(T* lhs)
	{
		(lhs != nullptr) ? offset = S_C(int32_t, (R_C(TBYTE*, lhs) - R_C(TBYTE*, this))) : offset = 0x7FFFFFFF;
		return lhs;
	}
	T* operator->()
	{
		return S_C(T*, *this);
	}
	const T* operator->() const
	{
		return S_C(const T*, *this);
	}
	bool operator==(T* rhs) const
	{
		return S_C(const T*, *this) == rhs;
	}
	bool operator!=(T* rhs) const
	{
		return S_C(const T*, *this) != rhs;
	}
});

/**
 * Template for non-null pointers
 * @tparam T class type
 */
template <typename T>
struct TNoNullablePtr
{
	using Type = T;
	using ValueType = T*;

	TNoNullablePtr() = default;
	explicit TNoNullablePtr(std::nullptr_t)
	{
	}
	TNoNullablePtr(T* handle)
		: m_ptr(handle)
	{
		if (m_ptr == nullptr)
		{
			HLVM_SEGFAULT_INLINE();
		}
	}
	~TNoNullablePtr() = default;
	TNoNullablePtr(const TNoNullablePtr& other)
		: m_ptr(other.m_ptr)
	{
		if (m_ptr == nullptr)
		{
			HLVM_SEGFAULT_INLINE();
		}
	}
	TNoNullablePtr(TNoNullablePtr&& other)
		: m_ptr(other.m_ptr)
	{
		other.m_ptr = nullptr;
		if (m_ptr == nullptr)
		{
			HLVM_SEGFAULT_INLINE();
		}
	}

	T* operator->()
	{
		return m_ptr;
	}

	const T* operator->() const
	{
		return m_ptr;
	}

	friend T& operator*(const TNoNullablePtr& handle)
	{
		if (handle.m_ptr == nullptr)
		{
			HLVM_SEGFAULT_INLINE();
		}
		return *(handle.m_ptr);
	}

	TNoNullablePtr& operator=(const TNoNullablePtr& other)
	{
		if (this != &other)
		{
			m_ptr = other.m_ptr;
			if (m_ptr == nullptr)
			{
				HLVM_SEGFAULT_INLINE();
			}
		}
		return *this;
	}

	template <typename U>
	TNoNullablePtr& operator=(const TNoNullablePtr<U>& other)
	{
		if (RC1<const void*>(this) != RC1<const void*>(&other))
		{
			m_ptr = SC1<TNoNullablePtr<U>::ValueType>(other.Get());
			if (m_ptr == nullptr)
			{
				HLVM_SEGFAULT_INLINE();
			}
		}
		return *this;
	}

	// Compare with nullptr
	bool operator==(std::nullptr_t) const
	{
		return m_ptr == nullptr;
	}
	bool operator!=(std::nullptr_t) const
	{
		return m_ptr != nullptr;
	}

	operator bool() const
	{
		return m_ptr != nullptr;
	}

	explicit operator T*() const
	{
		return m_ptr;
	}

	T* Get() const
	{
		return m_ptr;
	}

private:
	T* m_ptr{ nullptr };
};

template <typename T>
using TNNPtr = TNoNullablePtr<T>;

/**
 * Template for nullable pointers
 * @tparam T class type
 */
template <typename T>
struct TNullablePtr
{
	using Type = T;
	using ValueType = T*;

	TNullablePtr() = default;
	TNullablePtr(T* handle)
		: m_ptr(handle)
	{
	}
	~TNullablePtr() = default;
	TNullablePtr(const TNullablePtr& other)
		: m_ptr(other.m_ptr)
	{
	}
	TNullablePtr(TNullablePtr&& other)
		: m_ptr(other.m_ptr)
	{
		other.m_ptr = nullptr;
	}

	T* operator->()
	{
		return m_ptr;
	}

	const T* operator->() const
	{
		return m_ptr;
	}

	// Compare with nullptr
	bool operator==(std::nullptr_t) const
	{
		return m_ptr == nullptr;
	}
	bool operator!=(std::nullptr_t) const
	{
		return m_ptr != nullptr;
	}

	operator bool() const
	{
		return m_ptr != nullptr;
	}

	explicit operator T*() const
	{
		return m_ptr;
	}

	friend T& operator*(const TNullablePtr& handle)
	{
		if (handle.m_ptr == nullptr)
		{
			HLVM_SEGFAULT_INLINE();
		}
		return *(handle.m_ptr);
	}

	TNullablePtr& operator=(std::nullptr_t)
	{
		m_ptr = nullptr;
		return *this;
	}

	TNullablePtr& operator=(const TNullablePtr& other)
	{
		if (this != &other)
		{
			m_ptr = other.m_ptr;
		}
		return *this;
	}

	template <typename U>
	TNullablePtr& operator=(const TNullablePtr<U>& other)
	{
		if (RC1<const void*>(this) != RC1<const void*>(&other))
		{
			m_ptr = SC1<TNullablePtr<U>::ValueType>(other.Get());
		}
		return *this;
	}

	TNullablePtr& operator=(const TNoNullablePtr<Type>& other)
	{
		if (RC1<const void*>(this) != RC1<const void*>(&other))
		{
			m_ptr = other.Get();
		}
		return *this;
	}

	template <typename U>
	TNullablePtr& operator=(const TNoNullablePtr<U>& other)
	{
		if (RC1<const void*>(this) != RC1<const void*>(&other))
		{
			m_ptr = SC1<TNoNullablePtr<U>::ValueType>(other.Get());
		}
		return *this;
	}

	T* Get() const
	{
		return m_ptr;
	}

private:
	T* m_ptr{ nullptr };
};
template <typename T>
using TNPtr = TNullablePtr<T>;
