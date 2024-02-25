/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/String.h"
#include "Core/Container/ContainerDefinition.h"

#include <boost/filesystem/path.hpp>

HLVM_ENUM(EPlatformFileType, uint8_t,
	Local,
	Packed,
	Unkown);

/**
 * boost path usage : https://blog.csdn.net/toby54king/article/details/81334962
 */
class FPath final : public boost::filesystem::path
{
public:
	FPath() = default;
	FPath(const char* str)
		: boost::filesystem::path(str)
	{
		ResolvePath();
	}
	FPath(const TCHAR* str)
		: boost::filesystem::path(reinterpret_cast<const char*>(str))
	{
		ResolvePath();
	}
	FPath(const boost::filesystem::path& str)
		: boost::filesystem::path(str)
	{
		ResolvePath();
	}
	FPath(const FString& str)
		: boost::filesystem::path(str.ToCharStr())
	{
		ResolvePath();
	}

	// Move, copy constructor
	FPath(FPath&& other) noexcept
		: boost::filesystem::path(MoveTemp(other))
	{
	}
	FPath(const FPath& other) noexcept
		: boost::filesystem::path(other)
	{
	}
	FPath& operator=(FPath&& other) noexcept
	{
		boost::filesystem::path::operator=(MoveTemp(other));
		return *this;
	}
	FPath& operator=(const FPath& other) noexcept
	{
		boost::filesystem::path::operator=(other);
		return *this;
	}

	// Convert to FString
	operator FString() const
	{
		return FString(ToCharStr());
	}

	// Convert to const TCHAR*
	operator const TCHAR*() const
	{
		return reinterpret_cast<const TCHAR*>(this->c_str());
	}
	friend const TCHAR* operator*(const FPath& fs)
	{
		return static_cast<const TCHAR*>(fs);
	}

	// Convert to const char*
	operator const char*() const
	{
		return this->c_str();
	}
	const char* ToCharStr() const
	{
		return static_cast<const char*>(*this);
	}

	size_t GetHash() const noexcept
	{
		if (mHash == 0)
		{
			mHash = CalculateHash();
		}
		return mHash;
	}

	FPath ChangeExtension(const FString& new_ext) const
	{
		FPath new_path = *this;
		new_path.replace_extension(new_ext.ToCharStr());
		return new_path;
	}

	FPath& ChangeExtension_Inplace(const FString& new_ext)
	{
		this->replace_extension(new_ext.ToCharStr());
		return *this;
	}

	/**
	 * Static methods, internally calling generic platform api
	 */
	static bool					 IsDirectory(const FPath& path);
	static bool					 Exists(const FPath& path);
	static TSmallVector32<FPath> FindAllMatch(const FPath& root_dir, const FString& regex, bool recursive = false);
	static FString				 DumpJson(const TSmallVector32<FPath>& paths);

	inline static boost::regex					 PathReplacePattern{ R"(\$\{([^}]+)\})" };
	inline static TMap<std::string, std::string> PathReplaceMap;

private:
	/**
	 * Resolve path ${XXX} with certain values
	 */
	void ResolvePath();

	/**
	 * Calculate hash
	 * @return Fast Hash value
	 */
	size_t CalculateHash() const noexcept;

	mutable size_t	  mHash{ 0 };
	EPlatformFileType mFileType{ EPlatformFileType::Unkown };
};

/*
	Custom hash function for FPath
*/
namespace std
{
	template <>
	struct hash<FPath>
	{
		std::size_t operator()(const FPath& path) const noexcept
		{
			return path.GetHash();
		}
	};
} // namespace std
