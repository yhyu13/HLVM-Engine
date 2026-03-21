/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "Platform/GenericPlatformFile.h"
#include "Platform/FileSystem/Path.h"
#include "Platform/FileSystem/Boost/BoostPlatformFile.h"
#include "Platform/FileSystem/Packed/PackedPlatformFile.h"

DECLARE_LOG_CATEGORY(LogGenericPlatformFile)

FGenericPlatformFile* FGenericPlatformFile::sPlatformFileRedirector[EPlatformFileType_NUM];

static FGenericPlatformFile SGenericPlatformFile{};

void FGenericPlatformFile::InternalInit()
{
	HLVM_ASSERT_F(!sPlatformFileRedirector[E2VALUE(EPlatformFileType::Unspecified)], TXT("Unknow Platform file is already registered"));
	sPlatformFileRedirector[E2VALUE(EPlatformFileType::Unspecified)] = &SGenericPlatformFile;
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
			FBoostPlatformFile::InternalInit();
			FPackedPlatformFile::InternalInit();
			FGenericPlatformFile::InternalInit();
		}
	});
	return sPlatformFileRedirector[E2VALUE(PlatformFileType)];
}

bool FGenericPlatformFile::IsDirectory(const FPath& path)
{
	if (path.Type() == EPlatformFileType::Disk)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Disk)])->IsDirectory(path);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FPackedPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Packed)])->IsDirectory(path);
	}
	else HLVM_LIKELY
	{
		bool ret;
		for (TUINT8 i = E2VALUE(EPlatformFileType::Unspecified) + 1; i < EPlatformFileType_NUM; i++)
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
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Disk)])->Exists(path);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FPackedPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Packed)])->Exists(path);
	}
	else HLVM_LIKELY
	{
		bool ret;
		for (TUINT8 i = E2VALUE(EPlatformFileType::Unspecified) + 1; i < EPlatformFileType_NUM; i++)
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
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Disk)])->Glob(path, regex, recursive);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FPackedPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Packed)])->Glob(path, regex, recursive);
	}
	else HLVM_LIKELY
	{
		TSmallVector32<FPath> ret;
		for (TUINT8 i = E2VALUE(EPlatformFileType::Unspecified) + 1; i < EPlatformFileType_NUM; i++)
		{
			TSmallVector32<FPath> ret1 = (sPlatformFileRedirector[i]->Glob(path, regex, recursive));
			ret.insert(ret.end(), ret1.begin(), ret1.end());
		}
		return ret;
	}
}

bool FGenericPlatformFile::SaveAsString(const FPath& path, const FString& content)
{
	if (path.Type() == EPlatformFileType::Disk)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Disk)])->SaveAsString(path, content);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FPackedPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Packed)])->SaveAsString(path, content);
	}
	else HLVM_LIKELY
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Disk)])->SaveAsString(path, content);
	}
}

bool FGenericPlatformFile::SaveAsStringArray(const FPath& path, const TVector<FString>& Result, const FString& linechanger)
{
	if (path.Type() == EPlatformFileType::Disk)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Disk)])->SaveAsStringArray(path, Result, linechanger);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FPackedPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Packed)])->SaveAsStringArray(path, Result, linechanger);
	}
	else HLVM_LIKELY
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Disk)])->SaveAsStringArray(path, Result, linechanger);
	}
}

bool FGenericPlatformFile::SaveAsByteArray(const FPath& path, const TVector<TBYTE>& content)
{
	if (path.Type() == EPlatformFileType::Disk)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Disk)])->SaveAsByteArray(path, content);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FPackedPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Packed)])->SaveAsByteArray(path, content);
	}
	else HLVM_LIKELY
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Disk)])->SaveAsByteArray(path, content);
	}
}

FString FGenericPlatformFile::LoadAsString(const FPath& path)
{
	if (path.Type() == EPlatformFileType::Disk)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Disk)])->LoadAsString(path);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FPackedPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Packed)])->LoadAsString(path);
	}
	else HLVM_LIKELY
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Disk)])->LoadAsString(path);
	}
}

TVector<FString> FGenericPlatformFile::LoadAsStringArray(const FPath& path, const TVector<FString>& delimiters)
{
	if (path.Type() == EPlatformFileType::Disk)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Disk)])->LoadAsStringArray(path, delimiters);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FPackedPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Packed)])->LoadAsStringArray(path, delimiters);
	}
	else HLVM_LIKELY
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Disk)])->LoadAsStringArray(path, delimiters);
	}
}

TVector<TBYTE> FGenericPlatformFile::LoadAsByteArray(const FPath& path)
{
	if (path.Type() == EPlatformFileType::Disk)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Disk)])->LoadAsByteArray(path);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FPackedPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Packed)])->LoadAsByteArray(path);
	}
	else HLVM_LIKELY
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Disk)])->LoadAsByteArray(path);
	}
}

bool FGenericPlatformFile::DeleteFile(const FPath& path)
{
	if (path.Type() == EPlatformFileType::Disk)
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Disk)])->DeleteFile(path);
	}
	else if (path.Type() == EPlatformFileType::Packed)
	{
		return S_C(FPackedPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Packed)])->DeleteFile(path);
	}
	else HLVM_LIKELY
	{
		return S_C(FBoostPlatformFile*, sPlatformFileRedirector[E2VALUE(EPlatformFileType::Disk)])->DeleteFile(path);
	}
}
