/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Platform/FileSystem/Boost/BoostFileSystem.h"

FBoostPlatformFile::FBoostPlatformFile()
{
}

bool FBoostPlatformFile::_IsDirectory(const FPath& path)
{
	FFileOpStatus				_Status;
	std::shared_ptr<IFFileStat> _Stat;
	mInnerFileHandle.Stat(_Stat, path, &_Status);
	return _Stat->IsDirectory();
}

bool FBoostPlatformFile::_Exists(const FPath& path)
{
	FFileOpStatus				_Status;
	std::shared_ptr<IFFileStat> _Stat;
	mInnerFileHandle.Stat(_Stat, path, &_Status);
	return _Stat->Exists();
}

TSVector32<FPath> FBoostPlatformFile::_FindAllMatch(const FPath& path, const FString& pattern, bool recursive)
{
	HLVM_ENSURE(false, TXT("not implemented"), *path, *pattern, recursive);
	return TSVector32<FPath>();
}
