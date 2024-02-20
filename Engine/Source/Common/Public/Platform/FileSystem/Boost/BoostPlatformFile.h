/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "BoostFileHandle.h"
#include "Platform/GenericPlatformFile.h"

class FBoostPlatformFile final : public FGenericPlatformFile
{
public:
	NOCOPYMOVE(FBoostPlatformFile)
	FBoostPlatformFile() = default;

	virtual bool				  IsDirectory(const FPath& path) final override;
	virtual bool				  Exists(const FPath& path) final override;
	virtual TSmallVector32<FPath> FindAllMatch(const FPath& root_dir, const FString& regex, bool recursive = false) final override;

private:
	FBoostFileHandle mDummyFileHandle; // Dummy file handle just to get file stat
};
