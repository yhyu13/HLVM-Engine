/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Platform/FileSystem/Path.h"
#include "Platform/GenericPlatformFile.h"

bool FPath::IsDirectory(const FPath& path)
{
	return FGenericPlatformFile::IsDirectory(path);
}

bool FPath::Exists(const FPath& path)
{
	return FGenericPlatformFile::Exists(path);
}

TSVector32<FPath> FPath::FindAllMatch(const FPath& path, const FString& pattern, bool recursive)
{
	return FGenericPlatformFile::FindAllMatch(path, pattern, recursive);
}
