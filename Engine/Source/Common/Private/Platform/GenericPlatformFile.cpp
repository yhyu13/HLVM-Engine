/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Platform/GenericPlatformFile.h"
#include "Platform/FileSystem/Path.h"

TSMap<EFilePlatformFileType, FGenericPlatformFile*> FGenericPlatformFile::sPlatformFileRedirector;

bool FGenericPlatformFile::IsDirectory(const FPath& path)
{
	return sPlatformFileRedirector[EFilePlatformFileType::Local]->_IsDirectory(path);
}

bool FGenericPlatformFile::Exists(const FPath& path)
{
	return sPlatformFileRedirector[EFilePlatformFileType::Local]->_Exists(path);
}

TSVector32<FPath> FGenericPlatformFile::FindAllMatch(const FPath& path, const FString& pattern, bool recursive)
{
	return sPlatformFileRedirector[EFilePlatformFileType::Local]->_FindAllMatch(path, pattern, recursive);
}
