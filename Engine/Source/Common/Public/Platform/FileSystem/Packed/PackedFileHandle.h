/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "PackedDefinition.h"
#include "PackedToken.h"
#include "PackedFragment.h"
#include "Platform/FileSystem/FileHandle.h"
#include "Core/Compress/Zstd.h"
#include "Core/Encrypt/RSA.h"

#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>

HLVM_ENUM(EPackedFileType, TUINT8,
	Base,
	Patch,
	Unkown);

/**
 * mapped region https://live.boost.org/doc/libs/1_83_0/doc/html/boost/interprocess/mapped_region.html
 */

class FPackedPlatformFile;
class FPackedFileHandle final : public IFileHandle
{
public:
	static constexpr FFileOptions sDefaultFileOptions{
		.eFileMode = EFileMode::RB,
		.eFileMapped = EFileMapped::Mapped,
		.eFileAsync = EFileAsync::NoAsync,
		.eFileLock = EFileLock::InterProcessLock
	};

	FPackedFileHandle() = default;
	~FPackedFileHandle() final override;

	virtual OpRetType				   Open(const FPath& FilePath, const FFileOptions& Options = sDefaultFileOptions) final override;
	virtual OpRetType				   Close() final override;
	HLVM_MAYBEUNUSED virtual OpRetType Read(void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx) final override;
	HLVM_MAYBEUNUSED virtual OpRetType Write(const void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx) final override;
	HLVM_MAYBEUNUSED virtual OpRetType Flush() final override;
	HLVM_MAYBEUNUSED virtual OpRetType Seek(int64_t Offset, EWhence Whence = EWhence::Begin) final override;
	HLVM_MAYBEUNUSED virtual OpRetType Tell(int64_t& Offset) final override;
	HLVM_MAYBEUNUSED virtual OpRetType Size(size_t& Size) final override;
	HLVM_MAYBEUNUSED virtual OpRetType Truncate(size_t Size) final override;
	HLVM_MAYBEUNUSED HLVM_NODISCARD virtual std::shared_ptr<IFFileStat> Stat(const FPath& FilePath) final override;

	friend bool operator>(const FPackedFileHandle& Lhs, const FPackedFileHandle& Rhs) noexcept
	{
		return Lhs.mMountOrder > Rhs.mMountOrder;
	}

private:
    friend FPackedPlatformFile;

	boost::interprocess::file_mapping					mContainerMappedFile;
	TVector<FPackedContainerFragment>					mContainerFragments;
	TMap<FPathHash, FPackedTokenEntryDataAndFragmentID> mTokenEntryFragmentMap;

	boost::interprocess::sharable_lock<boost::interprocess::file_lock> mTokenFileLock;
	boost::interprocess::sharable_lock<boost::interprocess::file_lock> mContainerFileLock;

	// Counting total opened packed entry handle that use data in this file handle
	// used to identify whether or not we can close this file
	std::atomic_uint_fast32_t mPackedEntryRefCount{ 0 };
	// Mounting order, the larger the prior when searching for files
	uint64_t mMountOrder{ 0 };
	// Identify packed file type
	EPackedFileType mPackedFileType{ EPackedFileType::Unkown };
};
