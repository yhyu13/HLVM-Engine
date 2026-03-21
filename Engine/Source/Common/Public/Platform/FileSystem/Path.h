/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "Core/String.h"
#include "Core/Container/ContainerDefinition.h"

#include <boost/filesystem/path.hpp>

HLVM_ENUM(EPlatformFileType, TUINT8,
	Unspecified,
	Disk,
	Packed);

/**
 * boost path usage : https://blog.csdn.net/toby54king/article/details/81334962
 */
using FPathHash = size_t;
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
		: boost::filesystem::path(str.ToCharCStr())
	{
		ResolvePath();
	}
	FPath(const char* str, EPlatformFileType FileType)
		: boost::filesystem::path(str), mFileType(FileType)
	{
		ResolvePath();
	}
	FPath(const TCHAR* str, EPlatformFileType FileType)
		: boost::filesystem::path(reinterpret_cast<const char*>(str)), mFileType(FileType)
	{
		ResolvePath();
	}
	FPath(const boost::filesystem::path& str, EPlatformFileType FileType)
		: boost::filesystem::path(str), mFileType(FileType)
	{
		ResolvePath();
	}
	FPath(const FString& str, EPlatformFileType FileType)
		: boost::filesystem::path(str.ToCharCStr()), mFileType(FileType)
	{
		ResolvePath();
	}

	// Move, copy constructor
	FPath(FPath&& other) noexcept
		: boost::filesystem::path(other), mHash(other.mHash), mFileType(other.mFileType)
	{
	}
	FPath(const FPath& other) noexcept
		: boost::filesystem::path(other), mHash(other.mHash), mFileType(other.mFileType)
	{
	}
	FPath& operator=(FPath&& other) noexcept
	{
		if (this != &other)
		{
			boost::filesystem::path::operator=(MoveTemp(other));
			mHash = MoveTemp(other.mHash);
			mFileType = MoveTemp(other.mFileType);
		}
		return *this;
	}
	FPath& operator=(const FPath& other) noexcept
	{
		if (this != &other)
		{
			boost::filesystem::path::operator=(other);
			mHash = other.mHash;
			mFileType = other.mFileType;
		}
		return *this;
	}

	// Convert to FString
	operator FString() const
	{
		return FString(ToCharCStr());
	}

	// Convert to const char*
	operator const char*() const
	{
		return this->c_str();
	}

	// Convert to const TCHAR*
	operator const TCHAR*() const
	{
		return reinterpret_cast<const TCHAR*>(this->c_str());
	}

	// Convert to const TCHAR* by left * operator (e.g. const TCHAR* path = *mypath)
	friend const TCHAR* operator*(const FPath& fs)
	{
		return static_cast<const TCHAR*>(fs);
	}

	// Convert to const char*
	const char* ToCharCStr() const
	{
		return static_cast<const char*>(*this);
	}

	// Convert to const TCHAR*
	const TCHAR* ToTCharCStr() const
	{
		return static_cast<const TCHAR*>(*this);
	}

	EPlatformFileType Type() const
	{
		return mFileType;
	}

	size_t GetHash() const noexcept
	{
		if (mHash == 0)
		{
			mHash = CalculateHash();
		}
		return mHash;
	}

	FPath  ChangeExtension(const FString& new_ext) const;
	FPath& ChangeExtensionInplace(const FString& new_ext);
	FPath  AppendExtension(const FString& new_ext) const;
	FPath& AppendExtensionInplace(const FString& new_ext);

public:
	/**
	 * Static methods, internally calling generic platform api
	 */
	static bool					 IsDirectory(const FPath& path);
	static bool					 Exists(const FPath& path);
	static TSmallVector32<FPath> Glob(const FPath& root_dir, const FString& regex, bool recursive = false);

	static FPath GetParentPath(const FPath& path)
	{
		return MoveTemp(FPath(path.parent_path()));
	}

	// variadic template of combining N paths
	template <typename... Args>
	static FPath Combine(const FPath& path1, const FPath& path2, Args&&... args)
	{
		auto  path = path1 / path2;
		FPath result{ path, (path1.mFileType == path2.mFileType ? path2.mFileType : EPlatformFileType::Unspecified) };
		if constexpr (sizeof...(args) > 0)
		{
			return Combine(result, ForwardTemp<Args>(args)...);
		}
		else
		{
			return result;
		}
	}

	static FPath   Absolute(const FPath& path);
	static FString GetExtension(const FPath& path);
	static FString GetBaseFileName(const FPath& path);
	static FString GetCleanFileName(const FPath& path);

public:
	HLVM_STATIC_VAR const FPath Empty;

private:
	/**
	 * Resolve path ${XXX} pattern with registered values
	 */
	void ResolvePath();

	/**
	 * Calculate hash
	 * @return Fast Hash value
	 */
	FPathHash CalculateHash() const noexcept;

private:
	// Path replace pattern, inspired by linux bash variable: ${...}, e.g. ${PROJECT_DIR}
	HLVM_STATIC_VAR const std::regex PathReplacePattern;
	// Path replace map: replace pattern with value, e.g. ${PROJECT_DIR} -> /Users/xxx/project
	HLVM_STATIC_VAR TMapSmall<std::string, std::string> PathReplaceProtocol;

private:
	mutable FPathHash mHash{ 0 };
	EPlatformFileType mFileType{ EPlatformFileType::Unspecified };
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
