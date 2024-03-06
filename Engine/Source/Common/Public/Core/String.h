/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once
#include "GlobalDefinition.h"
#include "Template/ReferenceTemplate.tpp"

#include <string>
#include <fmt/xchar.h>
#include <fmt/format.h>

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
class FStdString final : public std::basic_string<char>
{
public:
	FStdString() = delete;
	// Move constructor
	explicit FStdString(std::basic_string<char>&& other) noexcept
		: std::basic_string<char>(MoveTemp(other))
	{
	}
	FStdString& operator=(FStdString&& other) noexcept
	{
		std::basic_string<char>::operator=(MoveTemp(other));
		return *this;
	}

	// Convert to const TCHAR*
	operator const TCHAR*() const
	{
		return reinterpret_cast<const TCHAR*>(this->data());
	}

	friend const TCHAR* operator*(const FStdString& fs)
	{
		return static_cast<const TCHAR*>(fs);
	}
};

template <size_t N, typename CHAR = TCHAR>
struct TCharArrayStr
{
	static constexpr size_t Capacity{ N };
	TCHAR					Buffer[Capacity + 1];
	size_t					Size{ 0 };

	TCharArrayStr()
	{
		std::memset(Buffer, 0, Capacity + 1);
	}

	// 构造函数接受一个C风格字符串进行初始化
	explicit TCharArrayStr(const CHAR* input)
	{
		Size = std::strlen(reinterpret_cast<const char*>(input));
		assert(Size <= Capacity);
		std::strncpy(reinterpret_cast<char*>(Buffer), reinterpret_cast<const char*>(input), Size);
		Buffer[Size + 1] = '\0'; // 确保总是以空字符结束
	}

	// 获取字符串内容
	const CHAR* c_str() const
	{
		return Buffer;
	}

	CHAR* data()
	{
		return Buffer;
	}
};
