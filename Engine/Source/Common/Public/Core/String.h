/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once
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
		: std::basic_string<TCHAR>(std::move(other))
	{
	}
	FString(const FString& other) noexcept
		: std::basic_string<TCHAR>(other)
	{
	}
	FString& operator=(FString&& other) noexcept
	{
		std::basic_string<TCHAR>::operator=(std::move(other));
		return *this;
	}
	FString& operator=(const FString& other) noexcept
	{
		std::basic_string<TCHAR>::operator=(other);
		return *this;
	}

	// Conver to const TCHAR*
	operator const TCHAR*() const
	{
		return reinterpret_cast<const TCHAR*>(this->c_str());
	}

	template <typename... Args>
	static FString Format(const TCHAR* format, Args&&... args)
	{
		return FString(fmt::format(format, std::forward<Args>(args)...));
	}
};
