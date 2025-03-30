/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Platform/GenericPlatformFile.h"
#include "Platform/FileSystem/Path.h"
#include "Platform/FileSystem/Boost/BoostPlatformFile.h"
#include "Platform/FileSystem/Packed/PackedPlatformFile.h"

DECLARE_LOG_CATEGORY(LogGenericPlatformFile)

FGenericPlatformFile* FGenericPlatformFile::sPlatformFileRedirector[EPlatformFileType_NUM];

static FGenericPlatformFile SGenericPlatformFile{};

void FGenericPlatformFile::_Init()
{
	HLVM_ASSERT_F(!sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Unkown)], TXT("Unkown Platform file is already registered"));
	sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Unkown)] = &SGenericPlatformFile;
	HLVM_LOG(LogGenericPlatformFile, debug, TXT("Init FGenericPlatformFile"));
}

FGenericPlatformFile* FGenericPlatformFile::Get(EPlatformFileType PlatformFileType)
{
	static std::once_flag once;
	std::call_once(once, []() {
		{
			/**
			 * Init all sub platform file here
			 */
			FBoostPlatformFile::_Init();
			FPackedPlatformFile::_Init();
			FGenericPlatformFile::_Init();
		}
	});
	return sPlatformFileRedirector[HLVM_ENUM_V_T(size_t, PlatformFileType)];
}

bool FGenericPlatformFile::IsDirectory(const FPath& path)
{
	if (path.Type() == EPlatformFileType::Disk)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Disk)])->IsDirectory(path);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FPackedPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Packed)])->IsDirectory(path);
	}
	else if (path.Type() == EPlatformFileType::Remote)
	{
		return false;
		// return S_C(FPackedPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Remote)])->IsDirectory(path);
	}
	else
	{
		// TODO
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Disk)])->IsDirectory(path);
	}
}

bool FGenericPlatformFile::Exists(const FPath& path)
{
	if (path.Type() == EPlatformFileType::Disk)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Disk)])->Exists(path);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FPackedPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Packed)])->Exists(path);
	}
	else
	{
		// TODO
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Disk)])->Exists(path);
	}
}

TSmallVector32<FPath> FGenericPlatformFile::Glob(const FPath& path, const FString& regex, bool recursive)
{
	if (path.Type() == EPlatformFileType::Disk)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Disk)])->Glob(path, regex, recursive);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FPackedPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Packed)])->Glob(path, regex, recursive);
	}
	else
	{
		// TODO
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Disk)])->Glob(path, regex, recursive);
	}
}
