/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Platform/FileSystem/FileHandle.h"
#include "Platform/FileSystem/Path.h"
#include "Core/Parallel/Lock.h"

#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/asio.hpp>
#include <fstream>

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
};

class FPackedFileHandle final : public IFileHandle
{
public:
	FPackedFileHandle() = default;
	~FPackedFileHandle() final override;

	virtual OpRetType Open(const FPath& FilePath, const FFileOptions& Options, OpStatusType Status_InOut) final override;
	virtual OpRetType Close(OpStatusType Status_InOut) final override;
	virtual OpRetType Read(void* Buffer, size_t Size, int64_t Offset, OpStatusType Status_InOut) final override;
	virtual OpRetType Write(const void* Buffer, size_t Size, int64_t Offset, OpStatusType Status_InOut) final override;
	virtual OpRetType Flush(OpStatusType Status_InOut) final override;
	virtual OpRetType Seek(int64_t Offset, EWhence Whence, OpStatusType Status_InOut) final override;
	virtual OpRetType Tell(int64_t& Offset, OpStatusType Status_InOut) final override;
	virtual OpRetType Size(size_t& Size, OpStatusType Status_InOut) final override;

	/**
	 * These methods can be static methods, but since we require inheritance, they have to be member virtual methods
	 */
	virtual OpRetType Truncate(size_t Size, OpStatusType Status_InOut) final override;
	virtual OpRetType Stat(std::shared_ptr<IFFileStat>& Stat, const FPath& FilePath, OpStatusType Status_InOut) final override;

private:
	const void* MappedFileCurPos_R(int64_t Offset) const;
	void*		MappedFileCurPos_W(int64_t Offset);
	void		HandleException(const OpStatusType& Status_InOut, const TCHAR* Function, const std::exception& Exception);
	void		HandleException2(const OpStatusType& Status_InOut, const TCHAR* Function);

private:
	FPath										 mFilePath;
	std::optional<boost::iostreams::mapped_file> mMappedFile;
	int64_t										 mMappedSeekPos{ 0 };
	BIT_FLAG(mOpened){ false };
};
