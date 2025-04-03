/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Core/Assert.h"
#include "Platform/FileSystem/Boost/BoostPlatformFile.h"

#include <regex>
#include <boost/filesystem/directory.hpp>

DECLARE_LOG_CATEGORY(LogBoostPlatformFile)

static FBoostPlatformFile SBoostPlatformFile{};

void FBoostPlatformFile::_Init()
{
	HLVM_ASSERT_F(!sPlatformFileRedirector[HLVM_ENUM_VALUE(EPlatformFileType::Disk)], TXT("Local Platform file is already registered"));
	sPlatformFileRedirector[HLVM_ENUM_VALUE(EPlatformFileType::Disk)] = FBoostPlatformFile::Get();
	HLVM_LOG(LogBoostPlatformFile, debug, TXT("Init FBoostPlatformFile"));
}

FBoostPlatformFile* FBoostPlatformFile::Get()
{
	return &SBoostPlatformFile;
}

bool FBoostPlatformFile::IsDirectory(const FPath& path)
{
	std::shared_ptr<FBoostFileStat> _Stat = SP_C(FBoostFileStat, mFileHandle.Stat(path));
	HLVM_ASSERT_F(mFileHandle, TXT("FBoostPlatformFile::IsDirectory() - Failed to stat file"));
	return _Stat->IsDirectory();
}

bool FBoostPlatformFile::Exists(const FPath& path)
{
	std::shared_ptr<FBoostFileStat> _Stat = SP_C(FBoostFileStat, mFileHandle.Stat(path));
	HLVM_ASSERT_F(mFileHandle, TXT("FBoostPlatformFile::Exists() - Failed to stat file"));
	return _Stat->Exists();
}

TSmallVector32<FPath> FBoostPlatformFile::Glob(const FPath& root_dir, const FString& regex, bool recursive)
{
	TSmallVector32<FPath> Result;
	std::regex			  Regex{ regex.ToCharCStr() };

	if (recursive)
	{
		size_t RECURSIVE_ALERT = 100;
		for (boost::filesystem::recursive_directory_iterator it(root_dir), end; it != end; ++it)
		{
			if (boost::filesystem::is_regular_file(it->path()) && std::regex_match(it->path().c_str(), Regex))
			{
				Result.push_back(FPath(it->path()));
			}
			if (Result.size() > RECURSIVE_ALERT)
			{
				RECURSIVE_ALERT += RECURSIVE_ALERT;
				HLVM_LOG(LogBoostPlatformFile, trace,
					TXT("FBoostPlatformFile::Glob : Recursive search exceed {} under path {}"), RECURSIVE_ALERT, *root_dir);
			}
		}
	}
	else
	{
		for (boost::filesystem::directory_iterator it(root_dir), end; it != end; ++it)
		{
			if (boost::filesystem::is_regular_file(it->path()) && std::regex_match(it->path().c_str(), Regex))
			{
				Result.push_back(FPath(it->path()));
			}
		}
	}

	return Result;
}
