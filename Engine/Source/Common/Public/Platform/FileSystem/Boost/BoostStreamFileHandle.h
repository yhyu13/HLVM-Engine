/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "Platform/FileSystem/FileHandle.h"
#include "Core/Parallel/Lock.h"

#include <boost/interprocess/sync/file_lock.hpp>
#include <fstream>

/**
 * Stream file handle is a heavy class and its mechanism is also heavy underneath.
 * Use mapped file handle for most cases unless failure.
 * But stream file handle might be faster on some system that does not have proper copy on write feature.
 */
class FBoostStreamFileHandle final : public IFileHandle
{
public:
	FBoostStreamFileHandle() = default;
	~FBoostStreamFileHandle() final override;

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

private:
	std::fstream								mFStream;
	mutable boost::interprocess::file_lock*		mFileLock{ nullptr };
	mutable std::optional<FRecursiveAtomicFlag> mRecursiveLock;
};
