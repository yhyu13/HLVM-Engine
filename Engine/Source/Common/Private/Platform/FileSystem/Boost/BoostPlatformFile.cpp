/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "Core/Assert.h"
#include "Platform/FileSystem/Boost/BoostPlatformFile.h"

#include <regex>
#include <boost/filesystem/directory.hpp>

DECLARE_LOG_CATEGORY(LogBoostPlatformFile)

void FBoostPlatformFile::InternalInit()
{
	HLVM_ASSERT_F(!sPlatformFileRedirector[E2VALUE(EPlatformFileType::Disk)], TXT("Local Platform file is already registered"));
	sPlatformFileRedirector[E2VALUE(EPlatformFileType::Disk)] = FBoostPlatformFile::Get();
	HLVM_LOG(LogBoostPlatformFile, debug, TXT("Init FBoostPlatformFile"));
}

TNoNullablePtr<FBoostPlatformFile> FBoostPlatformFile::Get()
{
	static FBoostPlatformFile SBoostPlatformFile{};
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

bool FBoostPlatformFile::SaveAsString(const FPath& path, const FString& content)
{
	FBoostMapFileHandle mFileHandle;
	if (mFileHandle.Open(path, GWriteOnlyFileOptions).IsOpen())
	{
		mFileHandle.Seek(0, EWhence::Begin);
		if (content.NumBytes() > 0)
		{
			mFileHandle.Write(content.GetData(), content.NumBytes());

			TINT64 size;
			mFileHandle.Seek(0, EWhence::End)
				.Tell(size);

			if (SC1<TSIZE>(size) != content.NumBytes())
			{
				HLVM_LOG(LogBoostPlatformFile, err,
					TXT("FBoostPlatformFile::SaveAsString : Failed to write file {}, size mismatch {} != {}"),
					*path, size, content.NumBytes());
				return false;
			}
			return true;
		}
		else
		{
			HLVM_LOG(LogBoostPlatformFile, warn, TXT("FBoostPlatformFile::SaveAsString : {} is empty"), *path);
		}
	}
	else
	{
		HLVM_LOG(LogBoostPlatformFile, err, TXT("FBoostPlatformFile::SaveAsString : Failed to open file {}"), *path);
	}
	return false;
}

bool FBoostPlatformFile::SaveAsStringArray(const FPath& path, const TVector<FString>& Result, const FString& linechanger)
{
	FString Concat = FString::Join(Result, [](const FString& core) { return core.ToTCharCStr(); }, linechanger);
	return SaveAsString(path, Concat);
}

bool FBoostPlatformFile::SaveAsByteArray(const FPath& path, const TVector<TBYTE>& content)
{
	FBoostMapFileHandle mFileHandle;
	if (mFileHandle.Open(path, GWriteOnlyFileOptions).IsOpen())
	{
		mFileHandle.Seek(0, EWhence::Begin);
		if (content.NumBytes() > 0)
		{
			mFileHandle.Write(content.GetData(), content.NumBytes());

			TINT64 size;
			mFileHandle.Seek(0, EWhence::End)
				.Tell(size);

			if (SC1<TSIZE>(size) != content.NumBytes())
			{
				HLVM_LOG(LogBoostPlatformFile, err,
					TXT("FBoostPlatformFile::SaveAsByteArray : Failed to write file {}, size mismatch {} != {}"),
					*path, size, content.NumBytes());
				return false;
			}
			return true;
		}
		else
		{
			HLVM_LOG(LogBoostPlatformFile, warn, TXT("FBoostPlatformFile::SaveAsByteArray : {} content is empty"), *path);
		}
	}
	else
	{
		HLVM_LOG(LogBoostPlatformFile, err, TXT("FBoostPlatformFile::SaveAsByteArray : Failed to open file {}"), *path);
	}
	return false;
}

FString FBoostPlatformFile::LoadAsString(const FPath& path)
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
			HLVM_LOG(LogBoostPlatformFile, err, TXT("FBoostPlatformFile::LoadAsString : {} is empty"), *path);
		}
	}
	else
	{
		HLVM_LOG(LogBoostPlatformFile, err, TXT("FBoostPlatformFile::LoadAsString : Failed to open file {}"), *path);
	}
	return FString();
}

TVector<FString> FBoostPlatformFile::LoadAsStringArray(const FPath& path, const TVector<FString>& delimiters)
{
	FString			 Content = LoadAsString(path);
	TVector<FString> Result;
	Result.Reserve(Result.Num() / 16);
	FString::Parse(Result, Content, [&delimiters](const TCHAR* str) -> TSIZE {
			for (const FString& delimiter : delimiters)
			{
				if (FString::Equals(str, delimiter, 1))
				{
					return delimiter.Num();
				}
			}
			return 0ul; }, true);
	return Result;
}

TVector<TBYTE> FBoostPlatformFile::LoadAsByteArray(const FPath& path)
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
			HLVM_LOG(LogBoostPlatformFile, err, TXT("FBoostPlatformFile::LoadAsByteArray : {} is empty"), *path);
		}
	}
	else
	{
		HLVM_LOG(LogBoostPlatformFile, err, TXT("FBoostPlatformFile::LoadAsByteArray : Failed to open file {}"), *path);
	}
	return TVector<TBYTE>();
}

bool FBoostPlatformFile::DeleteFile(const FPath& path)
{
	boost::system::error_code ec;
	if (!boost::filesystem::remove(path, ec))
	{
		// Log
		HLVM_LOG(LogBoostPlatformFile, err, TXT("FBoostPlatformFile::DeleteFile : Failed to delete file {} with error {}"),
			*path, TCHARSTR(ec.message().c_str()));
		return false;
	}
	return true;
}
