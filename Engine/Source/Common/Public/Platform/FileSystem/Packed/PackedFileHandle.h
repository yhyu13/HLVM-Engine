/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "PackedDefinition.h"
#include "Platform/FileSystem/FileHandle.h"
#include "Core/Container/ContainerDefinition.h"
#include "Core/Compress/CompressDefinition.h"
#include "Core/Encrypt/EncryptDefinition.h"

#include <boost/iostreams/device/mapped_file.hpp>

HLVM_ENUM(EPackedFileType, uint8_t,
	Base,
	Patch,
	Unkown);

/**
 * Region of a file in a Cot file.
 */
struct FPackedTokenEntryData
{
	size_t		  StartPos;
	size_t		  Size;
	size_t		  DecompressSize;
	EEncryptType  EncryptType;
	ECompressType CompressType;
};

/**
 * Token data structure represented by each json object
 */
struct FPackedTokenEntry
{
	FPath				  Path;
	FPackedTokenEntryData Region;
};

/**
 * mapped region https://live.boost.org/doc/libs/1_83_0/doc/html/boost/interprocess/mapped_region.html
 */

class FPackedFileHandle final : public IFileHandle
{
public:
	static constexpr FFileOptions sPackedFileOptions{
		.eFileMode = EFileMode::RB,
		.eFileMapped = EFileMapped::Mapped,
		.eFileAsync = EFileAsync::NoAsync,
		.eFileLock = EFileLock::NoLock
	};

	static constexpr FFileSeekCtx sPackedFileSeekCtx{
		.Whence = EWhence::Begin,
		.bResetPos = false,
		.bEraseSeekPos = false,
	};

	FPackedFileHandle() = default;
	~FPackedFileHandle() final override;

	virtual OpRetType Open(const FPath& FilePath, const FFileOptions& Options = sPackedFileOptions) final override;
	virtual OpRetType Close() final override;
	virtual OpRetType Read(void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx = sPackedFileSeekCtx) final override;
	virtual OpRetType Write(const void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx = sPackedFileSeekCtx) final override;
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
	const void* MappedFileCurPos_R(int64_t Offset) const;

	TMap<FPath, FPackedTokenEntry> mTokenEntryMap;
	boost::iostreams::mapped_file  mContainerMappedFile;
	EPackedFileType				   mPackedFileType{ EPackedFileType::Unkown };
};
