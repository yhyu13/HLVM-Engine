/**
 * Copyright (c) 2024. MIT License. All rights reserved.
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

	virtual bool			  IsDirectory(const FPath& path);
	virtual bool			  Exists(const FPath& path);
	virtual TSVector32<FPath> FindAllMatch(const FPath& root_dir, const FString& regex, bool recursive = false);

	static FGenericPlatformFile* Get();

protected:
	static TSMap<EPlatformFileType, FGenericPlatformFile*> sPlatformFileRedirector;
};
