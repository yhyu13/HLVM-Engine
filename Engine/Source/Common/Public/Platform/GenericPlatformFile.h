/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "GenericPlatform.h"
#include "Platform/FileSystem/FileSystem.h"
#include "Core/Container/ContainerDefinition.h"

class FGenericPlatformFile
{
public:
	NOCOPYMOVE(FGenericPlatformFile)
	FGenericPlatformFile() = default;
	virtual ~FGenericPlatformFile() = default;

	virtual bool				  IsDirectory(const FPath& path);
	virtual bool				  Exists(const FPath& path);
	virtual TSmallVector32<FPath> Glob(const FPath& root_dir, const FString& regex, bool recursive = false);

	static FGenericPlatformFile* Get(EPlatformFileType PlatformFileType = EPlatformFileType::Unknow);

protected:
	static FGenericPlatformFile* sPlatformFileRedirector[EPlatformFileType_NUM];

private:
	static void _Init();
};
