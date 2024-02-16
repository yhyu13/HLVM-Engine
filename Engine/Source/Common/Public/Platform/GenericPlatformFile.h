/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "GenericPlatform.h"
#include "Platform/FileSystem/FileSystem.h"
#include "Core/Container/ContainerDefinition.h"

enum class EFilePlatformFileType : uint8_t
{
	Local,
	Packed,
	Network,
	GPU,
};

class FGenericPlatformFile
{
public:
	virtual ~FGenericPlatformFile() = default;

	static bool				 IsDirectory(const FPath& path);
	static bool				 Exists(const FPath& path);
	static TSVector32<FPath> FindAllMatch(const FPath& path, const FString& pattern, bool recursive = false);

protected:
	virtual bool			  _IsDirectory(const FPath& path) = 0;
	virtual bool			  _Exists(const FPath& path) = 0;
	virtual TSVector32<FPath> _FindAllMatch(const FPath& path, const FString& pattern, bool recursive = false) = 0;

	static TSMap<EFilePlatformFileType, FGenericPlatformFile*> sPlatformFileRedirector;
};
