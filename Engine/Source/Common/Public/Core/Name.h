/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once
#include "String.h"

#include "Core/Object/RefCountPtr.h"

class FName
{
	struct FNameInternal : public FRefCountable
	{
		FNameInternal() = default;
		explicit FNameInternal(const FString& Other)
			: Name(CopyTemp(Other))
		{
		}
		explicit FNameInternal(FString&& Other)
			: Name(MoveTemp(Other))
		{
		}

		FString Name;
	};

public:
	FName() = default;
	~FName() = default;

	FName(const char* str)
		: mInternal(new FNameInternal{ str })
	{
	}

	FName(const FString& str)
		: mInternal(new FNameInternal{ CopyTemp(str) })
	{
	}
	FName(FString&& str)
		: mInternal(new FNameInternal{ MoveTemp(str) })
	{
	}
	FName(const FName& other) noexcept
		: mInternal(CopyTemp(other.mInternal))
	{
	}
	FName(FName&& other) noexcept
		: mInternal(MoveTemp(other.mInternal))
	{
	}
	FName& operator=(const FName& Other) noexcept
	{
		if (this != &Other)
		{
			mInternal = CopyTemp(Other.mInternal);
		}
		return *this;
	}
	FName& operator=(FName&& Other) noexcept
	{
		if (this != &Other)
		{
			mInternal = MoveTemp(Other.mInternal);
		}
		return *this;
	}

	FString ToString() const
	{
		if (mInternal.Valid())
		{
			return mInternal->Name;
		}
		return FString{};
	}

	const char* ToCharCStr() const
	{
		if (mInternal.Valid())
		{
			return mInternal->Name.ToCharCStr();
		}
		return "";
	}

	size_t RefCount() const
	{
		if (mInternal.Valid())
		{
			return mInternal->RefCount();
		}
		return 0;
	}

private:
	TRefCountPtr<FNameInternal> mInternal;
};
