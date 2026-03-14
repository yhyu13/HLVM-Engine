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
	HLVM_ASSERT_F(!sPlatformFileRedirector[HLVM_E2VALUE(EPlatformFileType::Unspecified)], TXT("Unknow Platform file is already registered"));
	sPlatformFileRedirector[HLVM_E2VALUE(EPlatformFileType::Unspecified)] = &SGenericPlatformFile;
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
	return sPlatformFileRedirector[HLVM_E2VALUE(PlatformFileType)];
}

bool FGenericPlatformFile::IsDirectory(const FPath& path)
{
	if (path.Type() == EPlatformFileType::Disk)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_E2VALUE(EPlatformFileType::Disk)])->IsDirectory(path);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FPackedPlatformFile*, sPlatformFileRedirector[HLVM_E2VALUE(EPlatformFileType::Packed)])->IsDirectory(path);
	}
	else
	{
		bool ret;
		for (TUINT8 i = HLVM_E2VALUE(EPlatformFileType::Unspecified) + 1; i < EPlatformFileType_NUM; i++)
		{
			ret = sPlatformFileRedirector[i]->IsDirectory(path);
			if (ret)
			{
				return true;
			}
		}
		return false;
	}
}

bool FGenericPlatformFile::Exists(const FPath& path)
{
	if (path.Type() == EPlatformFileType::Disk)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_E2VALUE(EPlatformFileType::Disk)])->Exists(path);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FPackedPlatformFile*, sPlatformFileRedirector[HLVM_E2VALUE(EPlatformFileType::Packed)])->Exists(path);
	}
	else
	{
		bool ret;
		for (TUINT8 i = HLVM_E2VALUE(EPlatformFileType::Unspecified) + 1; i < EPlatformFileType_NUM; i++)
		{

			ret = sPlatformFileRedirector[i]->Exists(path);
			if (ret)
			{
				return true;
			}
		}

		return false;
	}
}

TSmallVector32<FPath> FGenericPlatformFile::Glob(const FPath& path, const FString& regex, bool recursive)
{
	if (path.Type() == EPlatformFileType::Disk)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_E2VALUE(EPlatformFileType::Disk)])->Glob(path, regex, recursive);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FPackedPlatformFile*, sPlatformFileRedirector[HLVM_E2VALUE(EPlatformFileType::Packed)])->Glob(path, regex, recursive);
	}
	else
	{
		TSmallVector32<FPath> ret;
		for (TUINT8 i = HLVM_E2VALUE(EPlatformFileType::Unspecified) + 1; i < EPlatformFileType_NUM; i++)
		{
			TSmallVector32<FPath> ret1 = (sPlatformFileRedirector[i]->Glob(path, regex, recursive));
			ret.insert(ret.end(), ret1.begin(), ret1.end());
		}
		return ret;
	}
}

FString FGenericPlatformFile::ReadFile(const FPath& path)
{
	if (path.Type() == EPlatformFileType::Disk)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_E2VALUE(EPlatformFileType::Disk)])->ReadFile(path);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FPackedPlatformFile*, sPlatformFileRedirector[HLVM_E2VALUE(EPlatformFileType::Packed)])->ReadFile(path);
	}
	else
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_E2VALUE(EPlatformFileType::Disk)])->ReadFile(path);
	}
}

TVector<TBYTE> FGenericPlatformFile::ReadContent(const FPath& path)
{
	if (path.Type() == EPlatformFileType::Disk)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_E2VALUE(EPlatformFileType::Disk)])->ReadContent(path);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FPackedPlatformFile*, sPlatformFileRedirector[HLVM_E2VALUE(EPlatformFileType::Packed)])->ReadContent(path);
	}
	else
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[HLVM_E2VALUE(EPlatformFileType::Disk)])->ReadContent(path);
	}
}
