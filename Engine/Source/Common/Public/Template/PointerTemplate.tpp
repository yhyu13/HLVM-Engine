/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

template <typename T>
struct TNoNullPointer
{
	using Type = T*;

	TNoNullPointer() = delete;
	explicit TNoNullPointer(T* handle)
		: pFileHandle(handle)
	{
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

private:
	T* pFileHandle = nullptr;
};
