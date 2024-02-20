/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Platform/FileSystem/FileHandle.h"
#include "Platform/FileSystem/Path.h"
#include "Core/Container/ContainerDefinition.h"

#include <boost/iostreams/device/mapped_file.hpp>

struct FTokEntry
{
public:
	FPath  Path;
	size_t StartPos;
	size_t Size;
};

class FPackedFileStat final : public IFFileStat
{
public:
	NOCOPYMOVE(FPackedFileStat)
	FPackedFileStat() = delete;
	explicit FPackedFileStat(const FPath& Path);

	virtual bool IsDirectory() const final override;
	virtual bool Exists() const final override;
	virtual bool IsFile() const final override;
	virtual bool IsLink() const final override;

private:
	boost::filesystem::file_status fs;
};

/**
 * boost file system  https://theboostcpplibraries.com/boost.filesystem-files-and-directories
 * boost interprocess file lock https://www.boost.org/doc/libs/1_84_0/doc/html/interprocess/synchronization_mechanisms.html#interprocess.synchronization_mechanisms.file_lock
 * boost mapped file https://beta.boost.org/doc/libs/1_83_0/libs/iostreams/doc/classes/mapped_file.html
 */

class FPackedTokCotFileHandle final : public IFileHandle
{
public:
	FPackedTokCotFileHandle() = default;
	~FPackedTokCotFileHandle() final override;

	virtual OpRetType Open(const FPath& FilePath, const FFileOptions& Options = FFileOptions(), OpStatusType Status_InOut = nullptr) final override;
	virtual OpRetType Close(OpStatusType Status_InOut = nullptr) final override;
	virtual OpRetType Read(void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx = FFileSeekCtx(), OpStatusType Status_InOut = nullptr) final override;
	virtual OpRetType Write(const void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx = FFileSeekCtx(), OpStatusType Status_InOut = nullptr) final override;
	virtual OpRetType Flush(OpStatusType Status_InOut = nullptr) final override;
	virtual OpRetType Seek(int64_t Offset, EWhence Whence = EWhence::Begin, OpStatusType Status_InOut = nullptr) final override;
	virtual OpRetType Tell(int64_t& Offset, OpStatusType Status_InOut = nullptr) final override;
	virtual OpRetType Size(size_t& Size, OpStatusType Status_InOut = nullptr) final override;

	/**
	 * These methods can be static methods, but since we require inheritance, they have to be member virtual methods
	 */
	virtual OpRetType								  Truncate(size_t Size, OpStatusType Status_InOut = nullptr) final override;
	[[nodiscard]] virtual std::shared_ptr<IFFileStat> Stat(const FPath& FilePath, OpStatusType Status_InOut = nullptr) final override;

private:
	void HandleException(const OpStatusType& Status_InOut, const TCHAR* Function, const std::exception& Exception);
	void HandleException2(const OpStatusType& Status_InOut, const TCHAR* Function);

private:
	FPath										 mTokFilePath;
	FPath										 mCotFilePath;
	TMap<FPath, >								 mTokMappedFile;
	std::optional<boost::iostreams::mapped_file> mCotMappedFile;
	BIT_FLAG(mOpened){ false };
};
