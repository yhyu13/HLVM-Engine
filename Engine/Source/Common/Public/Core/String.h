/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once
#include "Template/GlobalTemplate.tpp"

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
#define TO_TCHAR_STR(x) (const TCHAR*)(x)

// https://stackoverflow.com/a/8488201
// Get the file name without path
#define __FILENAME__ FString(strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

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

	template <typename... Args>
	static FString Format(const TCHAR* _format, Args&&... args)
	{
		return FString(MoveTemp(fmt::format(_format, std::forward<Args>(args)...)));
	}

	// Convert to const TCHAR*
	operator const TCHAR*() const
	{
		return this->c_str();
	}
	friend const TCHAR* operator*(const FString& fs)
	{
		return (const TCHAR*)fs;
	}

	// Convert to const TCHAR*
	operator const char*() const
	{
		return reinterpret_cast<const char*>(this->c_str());
	}
	const char* ToCharStr() const
	{
		return (const char*)(*this);
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

	// Conver to const TCHAR*
	operator const TCHAR*() const
	{
		return reinterpret_cast<const TCHAR*>(this->data());
	}

	friend const TCHAR* operator*(const FCharStringView& fs)
	{
		return (const TCHAR*)fs;
	}
};
