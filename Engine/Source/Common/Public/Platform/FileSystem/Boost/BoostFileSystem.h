/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "BoostFileHandle.h"
#include "Platform/GenericPlatformFile.h"

class FBoostPlatformFile final : public FGenericPlatformFile
{
public:
	FBoostPlatformFile();

protected:
	virtual bool			  _IsDirectory(const FPath& path) final override;
	virtual bool			  _Exists(const FPath& path) final override;
	virtual TSVector32<FPath> _FindAllMatch(const FPath& path, const FString& pattern, bool recursive = false) final override;

private:
	FBoostFileHandle mInnerFileHandle;
};
