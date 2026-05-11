/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once
#include "GlobalDefinition.h"
#include "Platform/PlatformDefinition.h"
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
	FString(const std::string& str)
		: std::basic_string<TCHAR>(reinterpret_cast<const TCHAR*>(str.c_str()))
	{
	}

	// Move, copy constructor
	FString(FString&& other) noexcept
		: std::basic_string<TCHAR>(other)
	{
	}
	FString(const FString& other) noexcept
		: std::basic_string<TCHAR>(other)
	{
	}
	FString& operator=(FString&& other) noexcept
	{
		if (this != &other)
		{
			std::basic_string<TCHAR>::operator=(MoveTemp(other));
		}
		return *this;
	}
	FString& operator=(const FString& other) noexcept
	{
		if (this != &other)
		{
			std::basic_string<TCHAR>::operator=(other);
		}
		return *this;
	}

	// Convert to const TCHAR*
	operator const TCHAR*() const
	{
		return this->c_str();
	}

	// operator *this
	friend const TCHAR* operator*(const FString& fs)
	{
		return static_cast<const TCHAR*>(fs);
	}

	// Convert to const TCHAR*
	const TCHAR* ToTCharCStr() const
	{
		return (*this);
	}

	// Convert to const char*
	operator const char*() const
	{
		return reinterpret_cast<const char*>(this->c_str());
	}

	// Convert to const char*
	const char* ToCharCStr() const
	{
		return static_cast<const char*>(*this);
	}

	TCHAR* GetData()
	{
		return this->data();
	}

	const TCHAR* GetData() const
	{
		return this->data();
	}

	bool IsEmpty() const
	{
		return this->size() == 0;
	}

	TSIZE Num() const
	{
		return this->size();
	}

	TSIZE NumBytes() const
	{
		return this->size() * sizeof(TCHAR);
	}

	FString ToLower() const
	{
		FString result = *this;
		for (auto& c : result)
		{
			c = static_cast<TCHAR>(std::tolower(c));
		}
		return result;
	}

	FString ToUpper() const
	{
		FString result = *this;
		for (auto& c : result)
		{
			c = static_cast<TCHAR>(std::toupper(c));
		}
		return result;
	}

	bool EndsWith(const FString& str) const
	{
		return this->size() >= str.size() && std::equal(str.rbegin(), str.rend(), this->rbegin());
	}

	bool RemoveFromEndsInplace(const FString& str)
	{
		if (this->EndsWith(str))
		{
			this->resize(this->size() - str.size());
			return true;
		}
		return false;
	}

	bool StartsWith(const FString& str) const
	{
		return this->size() >= str.size() && std::equal(str.begin(), str.end(), this->begin());
	}

	bool RemoveFromStarts(const FString& str)
	{
		if (this->StartsWith(str))
		{
			this->erase(0, str.size());
			return true;
		}
		return false;
	}

	template <typename... Args>
	static FString Format(const TCHAR* _format, Args&&... args)
	{
		return FString(MoveTemp(fmt::format(_format, std::forward<Args>(args)...)));
	}

	template <typename VecType, typename PredType>
	static FString Join(const VecType& Vec,
		const PredType&				   func,
		const FString&				   splitter = TXT(",\n"))
	{
		FString result;
		int32_t count = 0;
		for (const auto& elem : Vec)
		{
			if (count++ > 0)
			{
				result += splitter;
			}
			result += static_cast<const TCHAR*>(func(elem));
		}
		return result;
	}

	// Parse
	template <typename VecType, typename PredType>
	static bool Parse(VecType& Result,
		const FString&		   Input,
		const PredType&		   ParserFunc,
		bool				   bEraseEmpty = true)
	{
		// Reserve a little bit
		Result.Reserve(Input.Num() / 32);
		TSIZE Index = 0;
		for (TSIZE i = 0; i < Input.Num();)
		{
			if (auto Offset = ParserFunc(&Input[i]); Offset > 0)
			{
				// If not empty
				if (i - Index >= 1)
				{
					TSIZE	len = i - Index + 1;
					FString tmp = new TCHAR[len];
					for (TSIZE j = 0; j < len - 1; ++j)
					{
						tmp[j] = Input[i + j];
					}
					tmp[len - 1] = TCHAR(0);
					Result.Add(MoveTemp(tmp));
				}
				else if (!bEraseEmpty)
				{
					Result.Add(FString{});
				}

				i += Offset;
				Index = i;
			}
			else
			{
				++i;
			}
		}
		return Result.Num() > 0;
	}

	static bool Equals(const TCHAR* lhs, const TCHAR* rhs, TSIZE len)
	{
		bool result = true;
		for (TSIZE i = 0; i < len; ++i)
		{
			if (lhs[i] != rhs[i])
			{
				result = false;
				break;
			}
		}
		return result;
	}
};

namespace std
{
	template <>
	struct hash<FString>
	{
		size_t operator()(const FString& str) const
		{
			return std::hash<std::basic_string<TCHAR>>{}(str);
		}
	};

	// template for ostream, istream
	inline std::ostream&& operator<<(std::ostream& os, const FString& str)
	{
		os << str.ToCharCStr();
		return std::move(os);
	}

	// template for istream, ostream
	inline std::istream&& operator>>(std::istream& is, FString& str)
	{
		std::string temp;
		is >> temp;
		str = FString(temp.c_str());
		return std::move(is);
	}
} // namespace std

HLVM_INLINE_FUNC bool operator==(const FString& lhs, const FString& rhs)
{
	return std::strcmp(lhs.ToCharCStr(), rhs.ToCharCStr()) == 0;
}

HLVM_INLINE_FUNC bool operator==(const FString& lhs, const char* rhs)
{
	return std::strcmp(lhs.ToCharCStr(), rhs) == 0;
}

HLVM_INLINE_FUNC bool operator==(const char* lhs, const FString& rhs)
{
	return std::strcmp(lhs, rhs.ToCharCStr()) == 0;
}

/**
 * FStdString is just a wrapper around a already allocated std::string
 * with our custom string api in addition, do not create a standalone FStdString
 */
class FStdString final : public std::basic_string<char>
{
public:
	FStdString() = delete;
	// Big Five
	FStdString(const FStdString& str)
		: std::basic_string<char>(str)
	{
	}

	// Move constructor
	explicit FStdString(std::basic_string<char>&& other) noexcept
		: std::basic_string<char>(other)
	{
	}
	FStdString& operator=(FStdString&& other) noexcept
	{
		std::basic_string<char>::operator=(other);
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
PACK(class TCharArray {
public:
	static constexpr size_t Capacity{ N };

	TCharArray() = default;

	TCharArray& operator=(const CHAR* input)
	{
		if (input)
		{
			Size = std::strlen(R_C(const char*, input));
			// Have to truncate size into capacity
			if (Size > Capacity)
			{
				Size = Capacity;
			}
			std::strncpy(R_C(char*, Buffer), R_C(const char*, input), Size);
			Buffer[Size + 1] = CHAR(0); // 确保总是以空字符结束
		}
		return *this;
	}

	const char* ToCharCStr() const
	{
		return reinterpret_cast<const char*>(c_str());
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

	const CHAR* data() const
	{
		return Buffer;
	}

	size_t size() const
	{
		return Size;
	}

	bool IsEmpty() const
	{
		return Size == 0;
	}

	// Convert to const CHAR*
	operator const CHAR*() const
	{
		return this->c_str();
	}

	// Convert to const char*
	operator const char*() const
	{
		return reinterpret_cast<const char*>(this->c_str());
	}

	friend const CHAR* operator*(const TCharArray<N, CHAR>& fs)
	{
		return fs.c_str();
	}

private:
	static_assert(sizeof(CHAR) / sizeof(char) == 1, "CHAR only support same size as char");
	CHAR   Buffer[Capacity + 1]{ 0 };
	size_t Size{ 0 };
});
