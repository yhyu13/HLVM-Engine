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
	HLVM_ASSERT_F(!sPlatformFileRedirector[HLVM_E2VALUE(EPlatformFileType::Disk)], TXT("Local Platform file is already registered"));
	sPlatformFileRedirector[HLVM_E2VALUE(EPlatformFileType::Disk)] = FBoostPlatformFile::Get();
	HLVM_LOG(LogBoostPlatformFile, debug, TXT("Init FBoostPlatformFile"));
}

TNoNullablePtr<FBoostPlatformFile> FBoostPlatformFile::Get()
{
	return &SBoostPlatformFile;
}

bool FBoostPlatformFile::IsDirectory(const FPath& path)
{
	std::shared_ptr<FBoostFileStat> _Stat = SP_C(FBoostFileStat, mDummyFileHandle.Stat(path));
	HLVM_ASSERT_F(mDummyFileHandle, TXT("FBoostPlatformFile::IsDirectory() - Failed to stat file"));
	return _Stat->IsDirectory();
}

bool FBoostPlatformFile::Exists(const FPath& path)
{
	std::shared_ptr<FBoostFileStat> _Stat = SP_C(FBoostFileStat, mDummyFileHandle.Stat(path));
	HLVM_ASSERT_F(mDummyFileHandle, TXT("FBoostPlatformFile::Exists() - Failed to stat file"));
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

FString FBoostPlatformFile::ReadFile(const FPath& path)
{
	FBoostMapFileHandle mFileHandle;
	if (mFileHandle.Open(path).IsOpen())
	{
		TINT64 size;
		mFileHandle.Seek(0, EWhence::End)
			.Tell(size)
			.Seek(0, EWhence::Begin);
		if (size > 0)
		{
			TSIZE		   size2 = static_cast<TSIZE>(size);
			TVector<TBYTE> Result;
			Result.resize(size2 + 1);
			mFileHandle.Read(Result.GetData(), size2);
			// Add null terminator
			if (*Result.LastData() != TBYTE{ 0 })
			{
				Result[Result.Size()] = TBYTE{ 0 };
			}
			return FString{ TO_TCHAR_CSTR(Result.GetData()) };
		}
		else
		{
			HLVM_LOG(LogBoostPlatformFile, err, TXT("FBoostPlatformFile::ReadFile : {} is empty"), *path);
		}
	}
	else
	{
		HLVM_LOG(LogBoostPlatformFile, err, TXT("FBoostPlatformFile::ReadFile : Failed to open file {}"), *path);
	}
	return FString();
}

TVector<TBYTE> FBoostPlatformFile::ReadContent(const FPath& path)
{
	FBoostMapFileHandle mFileHandle;
	if (mFileHandle.Open(path).IsOpen())
	{
		TINT64 size;
		mFileHandle.Seek(0, EWhence::End)
			.Tell(size)
			.Seek(0, EWhence::Begin);
		if (size > 0)
		{
			TSIZE		   size2 = static_cast<TSIZE>(size);
			TVector<TBYTE> Result;
			Result.resize(size2);
			mFileHandle.Read(Result.GetData(), size2);
			return MoveTemp(Result);
		}
		else
		{
			HLVM_LOG(LogBoostPlatformFile, err, TXT("FBoostPlatformFile::ReadContent : {} is empty"), *path);
		}
	}
	else
	{
		HLVM_LOG(LogBoostPlatformFile, err, TXT("FBoostPlatformFile::ReadContent : Failed to open file {}"), *path);
	}
	return TVector<TBYTE>();
}
