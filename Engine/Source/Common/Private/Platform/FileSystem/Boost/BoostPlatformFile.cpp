/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Assert.h"
#include "Platform/FileSystem/Boost/BoostPlatformFile.h"

#include <boost/regex.hpp>
#include <boost/filesystem/directory.hpp>

DELCARE_LOG_CATEGORY(LogBoostPlatformFile)
DEFINE_LOG_CATEGORY(LogBoostPlatformFile)

void FBoostPlatformFile::Init()
{
	HLVM_ASSERT(!sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Local)], TXT("Platform file is already registered"));
	sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Local)] = new FBoostPlatformFile();
	HLVM_LOG(LogBoostPlatformFile, debug, TXT("FGenericPlatformFile init FBoostPlatformFile"));
}

bool FBoostPlatformFile::IsDirectory(const FPath& path)
{
	std::shared_ptr<FBoostFileStat> _Stat = SP_C(FBoostFileStat, mFileHandle.Stat(path));
	HLVM_ASSERT(mFileHandle, TXT("FBoostPlatformFile::IsDirectory() - Failed to stat file"));
	return _Stat->IsDirectory();
}

bool FBoostPlatformFile::Exists(const FPath& path)
{
	std::shared_ptr<FBoostFileStat> _Stat = SP_C(FBoostFileStat, mFileHandle.Stat(path));
	HLVM_ASSERT(mFileHandle, TXT("FBoostPlatformFile::Exists() - Failed to stat file"));
	return _Stat->Exists();
}

TSmallVector32<FPath> FBoostPlatformFile::Find(const FPath& root_dir, const FString& regex, bool recursive)
{
	TSmallVector32<FPath> Result;
	boost::regex		  Regex{ regex.ToCharStr() };

	if (recursive)
	{
		size_t RECURSIVE_ALERT = 100;
		for (boost::filesystem::recursive_directory_iterator it(root_dir), end; it != end; ++it)
		{
			if (boost::filesystem::is_regular_file(it->path()) && boost::regex_match(it->path().c_str(), Regex))
			{
				Result.push_back(FPath(it->path()));
			}
			if (Result.size() > RECURSIVE_ALERT)
			{
				RECURSIVE_ALERT += RECURSIVE_ALERT;
				HLVM_LOG(LogBoostPlatformFile, debug,
					TXT("FBoostPlatformFile::Find() - Recursive search exceed {} for {}?"), RECURSIVE_ALERT, *root_dir);
			}
		}
	}
	else
	{
		for (boost::filesystem::directory_iterator it(root_dir), end; it != end; ++it)
		{
			if (boost::filesystem::is_regular_file(it->path()) && boost::regex_match(it->path().c_str(), Regex))
			{
				Result.push_back(FPath(it->path()));
			}
		}
	}

	return Result;
}
