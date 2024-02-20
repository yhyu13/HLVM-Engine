/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Platform/GenericPlatformFile.h"
#include "Platform/FileSystem/Path.h"
#include "Platform/FileSystem/Boost/BoostPlatformFile.h"

DELCARE_LOG_CATEGORY(LogGenericPlatformFile)
DEFINE_LOG_CATEGORY(LogGenericPlatformFile)

FGenericPlatformFile* FGenericPlatformFile::sPlatformFileRedirector[EPlatformFileType_NUM];

FGenericPlatformFile* FGenericPlatformFile::Get()
{
	static FGenericPlatformFile File;
	static std::once_flag		once;
	std::call_once(once, []() {
		{
			HLVM_ASSERT(!sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Local)], TXT("Platform file is already registered"));
			sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Local)] = new FBoostPlatformFile();
			HLVM_LOG(LogGenericPlatformFile, debug, TXT("FGenericPlatformFile init FBoostPlatformFile"));
		}
	});
	return &File;
}

bool FGenericPlatformFile::IsDirectory(const FPath& path)
{
	return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Local)])->IsDirectory(path);
}

bool FGenericPlatformFile::Exists(const FPath& path)
{
	return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Local)])->Exists(path);
}

TSmallVector32<FPath> FGenericPlatformFile::FindAllMatch(const FPath& path, const FString& regex, bool recursive)
{
	return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Local)])->FindAllMatch(path, regex, recursive);
}
