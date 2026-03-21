/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "Platform/FileSystem/Boost/BoostFileStat.h"
#include "Core/Assert.h"

FBoostFileStat::FBoostFileStat(const FPath& Path)
{
	mFs = boost::filesystem::status(Path.ToCharCStr());
}

bool FBoostFileStat::IsDirectory() const
{
	return boost::filesystem::is_directory(mFs);
}

bool FBoostFileStat::Exists() const
{
	return boost::filesystem::exists(mFs);
}

bool FBoostFileStat::IsFile() const
{
	return boost::filesystem::is_regular_file(mFs)
		&& !boost::filesystem::is_directory(mFs)
		&& !boost::filesystem::is_symlink(mFs);
}

bool FBoostFileStat::IsLink() const
{
	return !boost::filesystem::is_regular_file(mFs)
		&& !boost::filesystem::is_directory(mFs)
		&& boost::filesystem::is_symlink(mFs);
}
