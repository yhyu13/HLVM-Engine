/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Platform/FileSystem/FileHandle.h"
#include "Core/Parallel/Lock.h"

#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/interprocess/sync/file_lock.hpp>

/**
 * boost file system  https://theboostcpplibraries.com/boost.filesystem-files-and-directories
 * boost interprocess file lock https://www.boost.org/doc/libs/1_84_0/doc/html/interprocess/synchronization_mechanisms.html#interprocess.synchronization_mechanisms.file_lock
 * boost mapped file https://beta.boost.org/doc/libs/1_83_0/libs/iostreams/doc/classes/mapped_file.html
 * boost std:fbuf init file before mapping https://stackoverflow.com/questions/70480239/boost-mmaping-a-file-into-memory-for-readswrites
 */
class FBoostMapFileHandle final : public IFileHandle
{
public:
	FBoostMapFileHandle() = default;
	~FBoostMapFileHandle() final override;

	virtual OpRetType Open(const FPath& FilePath, const FFileOptions& Options = GReadOnlyFileOptions) final override;
	virtual OpRetType Close() final override;
	virtual OpRetType Read(void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx = GFileSeekCurCtx) final override;
	virtual OpRetType Write(const void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx = GFileSeekCurCtx) final override;
	virtual OpRetType Flush() final override;
	virtual OpRetType Seek(int64_t Offset, EWhence Whence = EWhence::Begin) final override;
	virtual OpRetType Tell(int64_t& Offset) final override;
	virtual OpRetType Size(size_t& Size) final override;

	virtual OpRetType								   Truncate(size_t Size) final override;
	HLVM_NODISCARD virtual std::shared_ptr<IFFileStat> Stat(const FPath& FilePath) final override;

	/**
	 * For read only mapped file, we can use this to get the buffer range
	 */
	HLVM_NODISCARD FConstByteBuffer GetMappedBufferReadOnly() const;

private:
	void		MappedFileInit();
	const void* MappedFileCurPos_R(int64_t Offset = 0) const;
	void*		MappedFileCurPos_W(int64_t Offset = 0);

private:
	boost::iostreams::mapped_file				mMappedFile;
	int64_t										mMappedSeekPos{ 0 };
	mutable boost::interprocess::file_lock*		mFileLock{ nullptr };
	mutable std::optional<FRecursiveAtomicFlag> mRecursiveLock;
	BIT_FLAG(mMappedLazyInit){ false };
};
