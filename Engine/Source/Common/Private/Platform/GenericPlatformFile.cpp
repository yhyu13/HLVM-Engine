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
	static FGenericPlatformFile* FilePtr = new FGenericPlatformFile();
	static std::once_flag		 once;
	std::call_once(once, []() {
		{
			/**
			 * Init all sub platform file here
			 */
			FBoostPlatformFile::Init();
		}
	});
	return FilePtr;
}

bool FGenericPlatformFile::IsDirectory(const FPath& path)
{
	if (path.Type() == EPlatformFileType::Local)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Local)])->IsDirectory(path);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Packed)])->IsDirectory(path);
	}
	else
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Local)])->IsDirectory(path);
	}
}

bool FGenericPlatformFile::Exists(const FPath& path)
{
	if (path.Type() == EPlatformFileType::Local)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Local)])->Exists(path);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Packed)])->Exists(path);
	}
	else
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Local)])->Exists(path);
	}
}

TSmallVector32<FPath> FGenericPlatformFile::Find(const FPath& path, const FString& regex, bool recursive)
{
	if (path.Type() == EPlatformFileType::Local)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Local)])->Find(path, regex, recursive);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Packed)])->Find(path, regex, recursive);
	}
	else
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Local)])->Find(path, regex, recursive);
	}
}
