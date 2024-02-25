/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "PackedDefinition.h"
#include "Platform/FileSystem/FileHandle.h"
#include "Core/Compress/Zstd.h"
#include "Core/Encrypt/RSA.h"

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
	uint32_t	  StartPos;
	uint32_t	  Size;
	uint32_t	  DecompressSize;
	EEncryptType  EncryptType{ EEncryptType::Unkown };
	ECompressType CompressType{ ECompressType::Unkown };
};

/**
 * Token data structure represented by each json object
 */
struct FPackedTokenEntry
{
	FPackedTokenEntryData Data;
	size_t				  PathHash; // RelativeToMountingPoint
};

HLVM_INLINE_VAR constexpr size_t FPackedTokenEntry_SerializedSize = sizeof(FPackedTokenEntry);
bool							 GetSerialized(const FPackedTokenEntry& Data, std::span<std::byte>& Buffer);
bool							 SetSerialized(FPackedTokenEntry& Data, const std::span<const std::byte>& Buffer);

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
		.eFileLock = EFileLock::NoLock
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

private:
	const void* MappedFileCurPos_R(int64_t Offset) const;

	TMap<FPath, FPackedTokenEntryData> mTokenEntryMap;
	boost::iostreams::mapped_file	   mContainerMappedFile;
	EPackedFileType					   mPackedFileType{ EPackedFileType::Unkown };
};
