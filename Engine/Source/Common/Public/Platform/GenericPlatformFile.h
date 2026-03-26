/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "GenericPlatform.h"
#include "Platform/FileSystem/FileSystem.h"
#include "Core/Container/ContainerDefinition.h"

class FGenericPlatformFile
{
public:
	NOCOPYMOVE(FGenericPlatformFile);
	FGenericPlatformFile() = default;
	virtual ~FGenericPlatformFile() = default;

	virtual bool				  IsDirectory(const FPath& path);
	virtual bool				  Exists(const FPath& path);
	virtual TSmallVector32<FPath> Glob(const FPath& root_dir, const FString& regex, bool recursive = false);

	virtual bool				  SaveAsString(const FPath& path, const FString& content);
	virtual bool				  SaveAsStringArray(const FPath& path, const TVector<FString>& Result, const FString& linechanger = TXT("\n"));
	virtual bool				  SaveAsByteArray(const FPath& path, const TVector<TBYTE>& content);

	virtual FString				  LoadAsString(const FPath& path);
	virtual TVector<FString>	  LoadAsStringArray(const FPath& path, const TVector<FString>& delimiters = {TXT("\n"), TXT("\r\n")});
	virtual TVector<TBYTE>		  LoadAsByteArray(const FPath& path);

	virtual bool				  DeleteFile(const FPath& path);

	static TNoNullablePtr<FGenericPlatformFile> Get(EPlatformFileType PlatformFileType = EPlatformFileType::Unspecified);

protected:
	static TNoNullablePtr<FGenericPlatformFile> sPlatformFileRedirector[EPlatformFileType_NUM];

private:
	static void InternalInit();
};
