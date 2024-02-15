/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Assert.h"

template <typename T>
struct TNoNullPointer
{
	using Type = T;
	using PointerType = T*;

	NOCOPYMOVE(TNoNullPointer)
	TNoNullPointer() = delete;
	TNoNullPointer(T* handle)
		: pFileHandle(handle)
	{
		HLVM_ENSURE(pFileHandle != nullptr, TXT("Handle is null"));
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

	friend T& operator*(const TNoNullPointer& handle)
	{
		return *(handle.pFileHandle);
	}

private:
	T* pFileHandle = nullptr;
};

// Function to check if two pointers overlap
static bool IsPointerOverlap(const void* ptr1, size_t size1, const void* ptr2)
{
	const char* cptr1 = reinterpret_cast<const char*>(ptr1);
	const char* cptr2 = reinterpret_cast<const char*>(ptr2);

	// Calculate the end pointer of ptr1 by adding its size
	const char* end_ptr1 = cptr1 + size1;

	// If end_ptr1 is less than ptr2, they do not overlap
	if (end_ptr1 <= cptr2)
		return false;

	// If ptr1 is greater than or equal to ptr2, and end_ptr1 is greater than ptr2,
	// then there is an overlap
	return (cptr1 <= cptr2 && end_ptr1 >= cptr2);
}

// To compare with another region, you can modify the function to take a second size:
static bool IsPointerOverlap2(const void* ptr1, size_t size1, const void* ptr2, size_t size2)
{
	const char* cptr1 = reinterpret_cast<const char*>(ptr1);
	const char* cptr2 = reinterpret_cast<const char*>(ptr2);

	// Calculate the end pointers for both regions
	const char* end_ptr1 = cptr1 + size1;
	const char* end_ptr2 = cptr2 + size2;

	// Check if any part of region 1 overlaps with region 2
	if ((cptr1 <= cptr2 && end_ptr1 >= cptr2) || (cptr1 <= end_ptr2 && end_ptr1 >= end_ptr2) || (cptr2 <= cptr1 && end_ptr2 >= cptr1) || (cptr2 <= end_ptr1 && end_ptr2 >= end_ptr1))
	{
		return true;
	}

	return false;
}
