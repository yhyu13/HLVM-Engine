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

TSmallVector32<FPath> FPath::FindAllMatch(const FPath& root_dir, const FString& regex, bool recursive)
{
	return FGenericPlatformFile::Get()->FindAllMatch(root_dir, regex, recursive);
}

FString FPath::DumpJson(const TSmallVector32<FPath>& paths)
{
	return FString::Join(
		paths, [](auto& item) { return FString::Format(TXT("\"{}\""), *item); }, TXT(",\n"));
}

void FPath::ResolvePath()
{
	if (boost::regex_match(this->ToCharStr(), PathReplacePattern))
	{
		HLVM_ASSERT(PathReplaceMap.size() > 0, TXT("PathReplaceMap is empty"));
		boost::smatch match;
		std::string	  result = this->ToCharStr();
		for (auto const& replacement : PathReplaceMap)
		{
			result = boost::regex_replace(result, PathReplacePattern, replacement.second, boost::match_default | boost::format_sed);
		}
		this->assign(MoveTemp(result));
	}
}

size_t FPath::CalculateHash() const noexcept
{
	size_t hash = this->size() ^ static_cast<size_t>(mFileType);
	auto   last_slash = std::find(this->rbegin(), this->rend(), "/");
	size_t start_index = (last_slash == this->rend()) ? 0 : this->size() - static_cast<size_t>(std::distance(this->rbegin(), last_slash)) - 1;
	for (size_t i = start_index; i < this->size(); ++i)
	{
		hash = (hash * 31) ^ static_cast<size_t>(this->c_str()[i]);
	}
	HLVM_LOG(LogFPath, trace, TXT("Hash {} return {}"), *(*this), hash);
	return hash;
}
