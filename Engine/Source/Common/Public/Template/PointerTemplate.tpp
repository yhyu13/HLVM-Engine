/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Assert.h"

template <typename T>
struct TNoNullPointer
{
	using Type = T;
	using ValueType = T*;

	TNoNullPointer() = delete;
	explicit TNoNullPointer(T* handle)
		: pFileHandle(handle)
	{
		HLVM_ENSURE(pFileHandle != nullptr, TXT("Pointer is null"));
	}

	T* operator->()
	{
		return pFileHandle;
	}

	const T* operator->() const
	{
		return pFileHandle;
	}

	bool operator==(const TNoNullPointer& other) const
	{
		return pFileHandle == other.pFileHandle;
	}

	bool operator!=(const TNoNullPointer& other) const
	{
		return pFileHandle != other.pFileHandle;
	}

	operator bool() const
	{
		return pFileHandle != nullptr;
	}

	T* Get() const
	{
		return pFileHandle;
	}

	friend T& operator*(const TNoNullPointer& handle)
	{
		return *(handle.pFileHandle);
	}

private:
	T* pFileHandle = nullptr;
};

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

/**
 * Use int32 offset to this pointer to represent another pointer,
 * approximately 1%~3% slower than using raw pointer
 * @CAUTION we strictly assume that TOffsetPtr32 represent some pointer range smaller than 2GB
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
