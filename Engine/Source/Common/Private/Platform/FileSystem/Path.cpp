/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Platform/FileSystem/Path.h"
#include "Platform/GenericPlatformFile.h"

DELCARE_LOG_CATEGORY(LogFPath)
DEFINE_LOG_CATEGORY(LogFPath)

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

FString FPath::DumpJson(const TSmallVector32<FPath>& paths)
{
	return FString::Join(
		paths, [](auto& item) { return FString::Format(TXT("\"{}\""), *item); }, TXT(",\n"));
}

void FPath::ResolvePath()
{
	if (boost::regex_match(this->ToCharStr(), PathReplacePattern))
		HLVM_UNLIKELY
		{
			HLVM_ASSERT(PathReplaceMap.size() > 0, TXT("PathReplaceMap is empty"));
			std::string result = this->ToCharStr();
			for (auto const& replacement : PathReplaceMap)
			{
				result = boost::regex_replace(result, PathReplacePattern, replacement.second, boost::match_default | boost::format_sed);
			}
			HLVM_LOG(LogFPath, trace, TXT("Path {} is resolved to {}"), *(*this), TO_TCHAR_STR(result.c_str()));
			this->assign(MoveTemp(result));
		}
}

FPathHash FPath::CalculateHash() const noexcept
{
	size_t hash = this->size();
	auto   last_slash = std::find(this->rbegin(), this->rend(), "/");
	size_t start_index = (last_slash == this->rend()) ? 0 : this->size() - static_cast<size_t>(std::distance(this->rbegin(), last_slash)) - 1;
	for (size_t i = start_index; i < this->size(); ++i)
	{
		hash = (hash * 31) ^ static_cast<size_t>(this->c_str()[i]);
	}
	HLVM_LOG(LogFPath, trace, TXT("Path {} hash return {}"), *(*this), hash);
	return hash;
}

FPath FPath::ChangeExtension(const FString& new_ext) const
{
	HLVM_ASSERT(new_ext[0] == TXT('.'), TXT("{} must start with '.'"), new_ext);
	FPath new_path = *this;
	new_path.replace_extension(new_ext.ToCharStr());
	return new_path;
}

FPath& FPath::ChangeExtension_Inplace(const FString& new_ext)
{
	HLVM_ASSERT(new_ext[0] == TXT('.'), TXT("{} must start with '.'"), new_ext);
	this->replace_extension(new_ext.ToCharStr());
	return *this;
}

FPath FPath::AppendExtension(const FString& new_ext) const
{
	HLVM_ASSERT(new_ext[0] == TXT('.'), TXT("{} must start with '.'"), new_ext);
	std::string new_path = this->string();
	new_path += new_ext;
	return FPath{ new_path, this->mFileType };
}
