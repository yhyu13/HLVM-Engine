/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Platform/FileSystem/Boost/BoostFileStat.h"
#include "Core/Assert.h"

FBoostFileStat::FBoostFileStat(const FPath& Path)
{
	boost::system::error_code ec;
	mFs = boost::filesystem::status(Path.ToCharStr(), ec);
	HLVM_ENSURE(!ec, TXT("File {} stat failed"), *Path);
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
