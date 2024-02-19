/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Platform/FileSystem/Path.h"
#include "Platform/GenericPlatformFile.h"

bool FPath::IsDirectory(const FPath& path)
{
	return FGenericPlatformFile::Get()->IsDirectory(path);
}

bool FPath::Exists(const FPath& path)
{
	return FGenericPlatformFile::Get()->Exists(path);
}

TSVector32<FPath> FPath::FindAllMatch(const FPath& root_dir, const FString& regex, bool recursive)
{
	return FGenericPlatformFile::Get()->FindAllMatch(root_dir, regex, recursive);
}
