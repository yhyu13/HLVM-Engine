/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "PackedFileHandle.h"

class FPackedEntryStat final : public IFFileStat
{
public:
	NOCOPYMOVE(FPackedEntryStat)
	FPackedEntryStat() = delete;
	explicit FPackedEntryStat(const FPath& Path);

	virtual bool IsDirectory() const final override;
	virtual bool Exists() const final override;
	virtual bool IsFile() const final override;
	virtual bool IsLink() const final override;
};

/**
 * Packed entry, when open, will get corresponding memory region of a file in a packed file
 * then decrypt and decompress into a dynamically allocated buffer
 */
class FPackedEntryHandle final : public IFileHandle
{
public:
	static constexpr FFileOptions sDefaultEntryOptions{
		.eFileMode = EFileMode::RB,
		.eFileMapped = EFileMapped::Mapped,
		.eFileAsync = EFileAsync::NoAsync,
		.eFileLock = EFileLock::NoLock
	};

	FPackedEntryHandle() = default;
	~FPackedEntryHandle() final override;

	virtual OpRetType				   Open(const FPath& FilePath, const FFileOptions& Options = sDefaultEntryOptions) final override;
	virtual OpRetType				   Close() final override;
	virtual OpRetType				   Read(void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx = FFileSeekCtx()) final override;
	HLVM_MAYBEUNUSED virtual OpRetType Write(const void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx = FFileSeekCtx()) final override;
	HLVM_MAYBEUNUSED virtual OpRetType Flush() final override;
	virtual OpRetType				   Seek(int64_t Offset, EWhence Whence = EWhence::Begin) final override;
	virtual OpRetType				   Tell(int64_t& Offset) final override;
	virtual OpRetType				   Size(size_t& Size) final override;
	HLVM_MAYBEUNUSED virtual OpRetType Truncate(size_t Size) final override;
	HLVM_MAYBEUNUSED HLVM_NODISCARD virtual std::shared_ptr<IFFileStat> Stat(const FPath& FilePath) final override;

private:
	TVector<TBYTE>		  mContentBuffer;
	FPackedEntryQuickFind mQuickFind;
};
