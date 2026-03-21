/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "Platform/FileSystem/Path.h"
#include "Platform/GenericPlatformFile.h"

DECLARE_LOG_CATEGORY(LogFPath)

// Static
const FPath Empty;
// Static
const std::regex FPath::PathReplacePattern{ R"(\$\{([^}]+)\})" };
// Static
TMapSmall<std::string, std::string> FPath::PathReplaceProtocol;

bool FPath::IsDirectory(const FPath& path)
{
	return FGenericPlatformFile::Get()->IsDirectory(path);
}

bool FPath::Exists(const FPath& path)
{
	return FGenericPlatformFile::Get()->Exists(path);
}

TSmallVector32<FPath> FPath::Glob(const FPath& root_dir, const FString& regex, bool recursive)
{
	return FGenericPlatformFile::Get()->Glob(root_dir, regex, recursive);
}

void FPath::ResolvePath()
{
	if (std::regex_match(this->ToCharCStr(), PathReplacePattern))
		HLVM_UNLIKELY
		{
			HLVM_ASSERT_F(PathReplaceProtocol.size() > 0, TXT("PathReplaceProtocol is empty"));
			std::string result = this->ToCharCStr();
			for (auto const& replacement : PathReplaceProtocol)
			{
				result = std::regex_replace(result, PathReplacePattern, replacement.second);
			}
			HLVM_LOG(LogFPath, trace, TXT("Path {} is resolved to {}"), *(*this), TO_TCHAR_CSTR(result.c_str()));
			this->assign(MoveTemp(result));
		}
}

FPathHash FPath::CalculateHash() const noexcept
{
	// 1. Hash by string size
	size_t hash = this->size();
	// 2. Hash by every size_t bytes
	size_t i = 0;
	for (; i + sizeof(size_t) < this->size(); i += sizeof(size_t))
	{
		hash = (hash * 31) ^ *R_C(const size_t*, this->c_str() + i);
	}
	// 3. Hash by reset of bytes
	for (; i < this->size(); ++i)
	{
		hash = (hash * 31) ^ S_C(size_t, this->c_str()[i]);
	}
	HLVM_LOG(LogFPath, trace, TXT("Path hash {},{}"), *(*this), hash);
	return hash;
}

FPath FPath::ChangeExtension(const FString& new_ext) const
{
	HLVM_ASSERT_F(new_ext[0] == TXT('.'), TXT("{} must start with '.'"), new_ext);
	FPath new_path = *this;
	new_path.replace_extension(new_ext.ToCharCStr());
	return new_path;
}

FPath& FPath::ChangeExtensionInplace(const FString& new_ext)
{
	HLVM_ASSERT_F(new_ext[0] == TXT('.'), TXT("{} must start with '.'"), new_ext);
	this->replace_extension(new_ext.ToCharCStr());
	return *this;
}

FPath FPath::AppendExtension(const FString& new_ext) const
{
	HLVM_ASSERT_F(new_ext[0] == TXT('.'), TXT("{} must start with '.'"), new_ext);
	std::string new_path = this->string();
	new_path += new_ext;
	return FPath{ boost::filesystem::path{ new_path }, this->mFileType };
}

FPath& FPath::AppendExtensionInplace(const FString& new_ext)
{
	HLVM_ASSERT_F(new_ext[0] == TXT('.'), TXT("{} must start with '.'"), new_ext);
	this->append(new_ext);
	return *this;
}

FPath FPath::Absolute(const FPath& path)
{
	if (path.is_absolute())
	{
		return path;
	}
	else
	{
		return FPath{ boost::filesystem::absolute(path), path.mFileType };
	}
}

FString FPath::GetExtension(const FPath& path)
{
	return path.extension().c_str();
}

FString FPath::GetBaseFileName(const FPath& path)
{
	FString CleanFileName = GetCleanFileName(path);
	FString Extension = GetExtension(path);
	// Remove extension from  path
	if (Extension.size() > 0)
	{
		CleanFileName.RemoveFromEndsInplace(Extension);
	}
	return CleanFileName;
}

FString FPath::GetCleanFileName(const FPath& path)
{
	return path.filename().c_str();
}
