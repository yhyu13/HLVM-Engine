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

class FPackedEntryHandle final : public IFileHandle
{
public:
	FPackedEntryHandle() = default;
	~FPackedEntryHandle() final override;

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
	boost::iostreams::mapped_file mMappedFile;
	int64_t						  mMappedSeekPos{ 0 };
};
