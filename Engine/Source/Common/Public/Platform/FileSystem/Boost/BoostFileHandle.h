/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Platform/FileSystem/FileHandle.h"
#include "Core/Parallel/Lock.h"

#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <fstream>

class FBoostFileStat final : public IFFileStat
{
public:
	NOCOPYMOVE(FBoostFileStat)
	FBoostFileStat() = delete;
	explicit FBoostFileStat(const FPath& Path);

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
 * boost std:fbuf init file before mapping https://stackoverflow.com/questions/70480239/boost-mmaping-a-file-into-memory-for-readswrites
 */

class FBoostFileHandle final : public IFileHandle, public FAtomicFlagStatic
{
public:
	FBoostFileHandle() = default;
	~FBoostFileHandle() final override;

	virtual OpRetType Open(const FPath& FilePath, const FFileOptions& Options = FFileOptions()) final override;
	virtual OpRetType Close() final override;
	virtual OpRetType Read(void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx = FFileSeekCtx()) final override;
	virtual OpRetType Write(const void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx = FFileSeekCtx()) final override;
	virtual OpRetType Flush() final override;
	virtual OpRetType Seek(int64_t Offset, EWhence Whence = EWhence::Begin) final override;
	virtual OpRetType Tell(int64_t& Offset) final override;
	virtual OpRetType Size(size_t& Size) final override;

	/**
	 * These methods can be static methods, but since we require inheritance, they have to be member virtual methods
	 */
	virtual OpRetType								  Truncate(size_t Size) final override;
	[[nodiscard]] virtual std::shared_ptr<IFFileStat> Stat(const FPath& FilePath) final override;

private:
	void		MappedFileLazyInit();
	const void* MappedFileCurPos_R(int64_t Offset = 0) const;
	void*		MappedFileCurPos_W(int64_t Offset = 0);

private:
	boost::iostreams::mapped_file*				mMappedFile{ nullptr };
	int64_t										mMappedSeekPos{ 0 };
	std::fstream*								mFStream{ nullptr };
	mutable std::optional<FRecursiveAtomicFlag> mRecursiveLock;
	mutable boost::interprocess::file_lock*		mFileLock{ nullptr };
	BIT_FLAG(mMappedLazyInit){ false };
};
