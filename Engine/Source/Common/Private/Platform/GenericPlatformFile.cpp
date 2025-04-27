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
	HLVM_ASSERT_F(!sPlatformFileRedirector[HLVM_ENUM_VALUE(EPlatformFileType::Unspecified)], TXT("Unknow Platform file is already registered"));
	sPlatformFileRedirector[HLVM_ENUM_VALUE(EPlatformFileType::Unspecified)] = &SGenericPlatformFile;
	HLVM_LOG(LogGenericPlatformFile, debug, TXT("Init FGenericPlatformFile"));
}

TNoNullablePtr<FGenericPlatformFile> FGenericPlatformFile::Get(EPlatformFileType PlatformFileType)
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
	return sPlatformFileRedirector[HLVM_ENUM_VALUE(PlatformFileType)];
}

bool FGenericPlatformFile::IsDirectory(const FPath& path)
{
	if (path.Type() == EPlatformFileType::Disk)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_VALUE(EPlatformFileType::Disk)])->IsDirectory(path);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FPackedPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_VALUE(EPlatformFileType::Packed)])->IsDirectory(path);
	}
	else
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_VALUE(EPlatformFileType::Disk)])->IsDirectory(path);
	}
}

bool FGenericPlatformFile::Exists(const FPath& path)
{
	if (path.Type() == EPlatformFileType::Disk)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_VALUE(EPlatformFileType::Disk)])->Exists(path);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FPackedPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_VALUE(EPlatformFileType::Packed)])->Exists(path);
	}
	else
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_VALUE(EPlatformFileType::Disk)])->Exists(path);
	}
}

TSmallVector32<FPath> FGenericPlatformFile::Glob(const FPath& path, const FString& regex, bool recursive)
{
	if (path.Type() == EPlatformFileType::Disk)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_VALUE(EPlatformFileType::Disk)])->Glob(path, regex, recursive);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FPackedPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_VALUE(EPlatformFileType::Packed)])->Glob(path, regex, recursive);
	}
	else
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_VALUE(EPlatformFileType::Disk)])->Glob(path, regex, recursive);
	}
}

FString FGenericPlatformFile::ReadFile(const FPath& path)
{
	if (path.Type() == EPlatformFileType::Disk)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_VALUE(EPlatformFileType::Disk)])->ReadFile(path);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FPackedPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_VALUE(EPlatformFileType::Packed)])->ReadFile(path);
	}
	else
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_VALUE(EPlatformFileType::Disk)])->ReadFile(path);
	}
}

TVector<TBYTE> FGenericPlatformFile::ReadContent(const FPath& path)
{
	if (path.Type() == EPlatformFileType::Disk)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_VALUE(EPlatformFileType::Disk)])->ReadContent(path);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FPackedPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_VALUE(EPlatformFileType::Packed)])->ReadContent(path);
	}
	else
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_ENUM_VALUE(EPlatformFileType::Disk)])->ReadContent(path);
	}
}

