/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Platform/GenericPlatformFile.h"
#include "Platform/FileSystem/Path.h"
#include "Platform/FileSystem/Boost/BoostPlatformFile.h"

TSMap<EPlatformFileType, FGenericPlatformFile*> FGenericPlatformFile::sPlatformFileRedirector;

FGenericPlatformFile* FGenericPlatformFile::Get()
{
	static FGenericPlatformFile File;
	static std::once_flag		once;
	std::call_once(once, []() {
		FBoostPlatformFile::Initialize();
	});
	return &File;
}

bool FGenericPlatformFile::IsDirectory(const FPath& path)
{
	return sPlatformFileRedirector[EPlatformFileType::Local]->IsDirectory(path);
}

bool FGenericPlatformFile::Exists(const FPath& path)
{
	return sPlatformFileRedirector[EPlatformFileType::Local]->Exists(path);
}

TSVector32<FPath> FGenericPlatformFile::FindAllMatch(const FPath& path, const FString& regex, bool recursive)
{
	return sPlatformFileRedirector[EPlatformFileType::Local]->FindAllMatch(path, regex, recursive);
}
