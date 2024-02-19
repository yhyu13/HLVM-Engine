/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once
#include "Template/ReferenceTemplate.tpp"

#include <string>
#include <fmt/xchar.h>
#include <fmt/format.h>
// Use char for best compatibility with other libraries
#define TCHAR char8_t
static_assert(sizeof(TCHAR) == sizeof(char), "TCHAR is not char");

//  Use utf8 for all string literal
//  U8_STRING("Hello World!")
#define U8_STRING(str) u8##str
#define TXT(str) U8_STRING(str)
#define STRTIFY(x) TXT(#x)
#define TO_TCHAR_STR(x) reinterpret_cast<const TCHAR*>((x))
#define TO_FSTRING(x) FString((x))

class FString final : public std::basic_string<TCHAR>
{
public:
	FString() = default;
	FString(const char* str)
		: std::basic_string<TCHAR>(reinterpret_cast<const TCHAR*>(str))
	{
	}
	FString(const TCHAR* str)
		: std::basic_string<TCHAR>(str)
	{
	}
	FString(const std::basic_string<TCHAR>& str)
		: std::basic_string<TCHAR>(str)
	{
	}

	// Move, copy constructor
	FString(FString&& other) noexcept
		: std::basic_string<TCHAR>(MoveTemp(other))
	{
	}
	FString(const FString& other) noexcept
		: std::basic_string<TCHAR>(other)
	{
	}
	FString& operator=(FString&& other) noexcept
	{
		std::basic_string<TCHAR>::operator=(MoveTemp(other));
		return *this;
	}
	FString& operator=(const FString& other) noexcept
	{
		std::basic_string<TCHAR>::operator=(other);
		return *this;
	}

	// Convert to const TCHAR*
	operator const TCHAR*() const
	{
		return this->c_str();
	}
	friend const TCHAR* operator*(const FString& fs)
	{
		return static_cast<const TCHAR*>(fs);
	}

	// Convert to const TCHAR*
	operator const char*() const
	{
		return reinterpret_cast<const char*>(this->c_str());
	}
	const char* ToCharStr() const
	{
		return static_cast<const char*>(*this);
	}

	template <typename... Args>
	static FString Format(const TCHAR* _format, Args&&... args)
	{
		return FString(MoveTemp(fmt::format(_format, std::forward<Args>(args)...)));
	}

	template <typename VecType, typename PredType>
	static FString Join(const VecType& Vec,
		const PredType&				   func,
		const TCHAR*				   splitter = TXT(",\n"))
	{
		FString result{ "[ " };
		int32_t count = 0;
		for (const auto& elem : Vec)
		{
			if (count++ > 0)
			{
				result += splitter;
			}
			result += static_cast<const TCHAR*>(func(elem));
		}
		result += TXT(" ]");
		return result;
	}
};

/**
 * FStdStringView is just a wrapper around a already allocated std::string whose sole purpose is to
 * be used as a const TCHAR* later, and thus avoid copying into FString.
 */
class FCharStringView final : public std::basic_string<char>
{
public:
	FCharStringView() = delete;
	// Move constructor
	explicit FCharStringView(std::basic_string<char>&& other) noexcept
		: std::basic_string<char>(MoveTemp(other))
	{
	}
	FCharStringView& operator=(FCharStringView&& other) noexcept
	{
		std::basic_string<char>::operator=(MoveTemp(other));
		return *this;
	}

	// Convert to const TCHAR*
	operator const TCHAR*() const
	{
		return reinterpret_cast<const TCHAR*>(this->data());
	}

	friend const TCHAR* operator*(const FCharStringView& fs)
	{
		return static_cast<const TCHAR*>(fs);
	}
};
