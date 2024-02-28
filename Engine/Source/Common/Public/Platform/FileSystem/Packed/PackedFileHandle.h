/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "PackedDefinition.h"
#include "PackedToken.h"
#include "Platform/FileSystem/FileHandle.h"
#include "Core/Compress/Zstd.h"
#include "Core/Encrypt/RSA.h"

#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <mio/mmap.hpp>

HLVM_ENUM(EPackedFileType, uint8_t,
	Base,
	Patch,
	Unkown);

/**
 * mapped region https://live.boost.org/doc/libs/1_83_0/doc/html/boost/interprocess/mapped_region.html
 */

class FPackedFileHandle final : public IFileHandle
{
public:
	static constexpr FFileOptions sDefaultFileOptions{
		.eFileMode = EFileMode::RB,
		.eFileMapped = EFileMapped::Mapped,
		.eFileAsync = EFileAsync::NoAsync,
		.eFileLock = EFileLock::InterProcessLock
	};

	static constexpr FFileSeekCtx sDefaultFileSeekCtx{
		.Whence = EWhence::Begin,
		.bResetPos = false,
		.bEraseSeekPos = false,
	};

	FPackedFileHandle() = default;
	~FPackedFileHandle() final override;

	virtual OpRetType Open(const FPath& FilePath, const FFileOptions& Options = sDefaultFileOptions) final override;
	virtual OpRetType Close() final override;
	virtual OpRetType Read(void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx = sDefaultFileSeekCtx) final override;
	virtual OpRetType Write(const void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx = sDefaultFileSeekCtx) final override;
	virtual OpRetType Flush() final override;
	virtual OpRetType Seek(int64_t Offset, EWhence Whence = EWhence::Begin) final override;
	virtual OpRetType Tell(int64_t& Offset) final override;
	virtual OpRetType Size(size_t& Size) final override;

	virtual OpRetType								  Truncate(size_t Size) final override;
	[[nodiscard]] virtual std::shared_ptr<IFFileStat> Stat(const FPath& FilePath) final override;

	//	/**
	//	 * Shrink mmap file to mimimal
	//	 */
	//	void Shrink();

	friend bool operator>(const FPackedFileHandle& Lhs, const FPackedFileHandle& Rhs) noexcept
	{
		return Lhs.mMountOrder > Rhs.mMountOrder;
	}

private:
	const void* MappedFileCurPos_R(int64_t Offset) const;

	mio::mmap_source												   mContainerMappedFile;
	TMap<FPathHash, FPackedTokenEntryData>							   mTokenEntryMap;
	boost::interprocess::sharable_lock<boost::interprocess::file_lock> mTokenFileLock;
	boost::interprocess::sharable_lock<boost::interprocess::file_lock> mContainerFileLock;
	EPackedFileType													   mPackedFileType{ EPackedFileType::Unkown };
	uint64_t														   mMountOrder{ 0 }; // Mounting order, the larger the prior when searching for files
};

HLVM_INLINE_VAR TVector<std::unique_ptr<FPackedFileHandle>> GMountedPackedFileHandles;
