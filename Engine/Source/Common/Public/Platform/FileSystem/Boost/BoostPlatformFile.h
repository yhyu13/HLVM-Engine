/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "BoostFileStat.h"
#include "BoostStreamFileHandle.h"
#include "BoostMapFileHandle.h"
#include "Platform/GenericPlatformFile.h"

class FBoostPlatformFile final : public FGenericPlatformFile
{
public:
	NOCOPYMOVE(FBoostPlatformFile);

	static TNoNullablePtr<FBoostPlatformFile> Get();

	virtual bool				  IsDirectory(const FPath& path) final override;
	virtual bool				  Exists(const FPath& path) final override;
	virtual TSmallVector32<FPath> Glob(const FPath& root_dir, const FString& regex, bool recursive = false) final override;

	virtual bool				  SaveAsString(const FPath& path, const FString& content) final override;
	virtual bool				  SaveAsStringArray(const FPath& path, const TVector<FString>& Result, const FString& linechanger = TXT("\n")) final override;
	virtual bool				  SaveAsByteArray(const FPath& path, const TVector<TBYTE>& content) final override;

	virtual FString				  LoadAsString(const FPath& path) final override;
	virtual TVector<FString>	  LoadAsStringArray(const FPath& path, const TVector<FString>& delimiters = {TXT("\n"), TXT("\r\n")}) final override;
	virtual TVector<TBYTE>		  LoadAsByteArray(const FPath& path) final override;

	virtual bool				  DeleteFile(const FPath& path) final override;

private:
	friend class FGenericPlatformFile;

	FBoostPlatformFile() = default;

	/**
	 * Called inside FGenericPlatformFile::Get()
	 * Intentionally internal
	 */
	static void InternalInit();

private:
	FBoostMapFileHandle mDummyFileHandle; // Dummy file handle just to get file stat
};
