/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "Platform/FileSystem/FileHandle.h"
#include "Core/Parallel/Lock.h"

#include <boost/filesystem/file_status.hpp>

class FBoostFileStat final : public IFFileStat
{
public:
	NOCOPYMOVE(FBoostFileStat);
	FBoostFileStat() = delete;
	explicit FBoostFileStat(const FPath& Path);

	virtual bool IsDirectory() const final override;
	virtual bool Exists() const final override;
	virtual bool IsFile() const final override;
	virtual bool IsLink() const final override;

private:
	boost::filesystem::file_status mFs;
};
