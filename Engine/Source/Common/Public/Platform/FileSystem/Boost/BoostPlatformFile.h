/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "BoostFileStat.h"
#include "BoostStreamFileHandle.h"
#include "BoostMapFileHandle.h"
#include "Platform/GenericPlatformFile.h"

class FBoostPlatformFile final : public FGenericPlatformFile
{
public:
	NOCOPYMOVE(FBoostPlatformFile)
	FBoostPlatformFile() = default;

	/**
	 * Called inside FGenericPlatformFile::Get()
	 * Intentionally internal
	 */
	static void _Init();

	static TNoNullablePtr<FBoostPlatformFile> Get();

	virtual bool				  IsDirectory(const FPath& path) final override;
	virtual bool				  Exists(const FPath& path) final override;
	virtual TSmallVector32<FPath> Glob(const FPath& root_dir, const FString& regex, bool recursive = false) final override;
	virtual FString				  ReadFile(const FPath& path) final override;
	virtual TVector<TBYTE>		  ReadContent(const FPath& path) final override;

private:
	FBoostMapFileHandle mDummyFileHandle; // Dummy file handle just to get file stat
};
