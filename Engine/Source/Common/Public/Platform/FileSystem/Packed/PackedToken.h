/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "Platform/FileSystem/Path.h"
#include "Core/Compress/CompressDefinition.h"
#include "Core/Encrypt/EncryptDefinition.h"

/**
 * Region of a file in a Cot file.
 */
struct FPackedTokenEntryData
{
	size_t		  StartPos;
	size_t		  Size;
	size_t		  DecompressSize;
	EEncryptType  EncryptType : 4 { EEncryptType::No };
	ECompressType CompressType : 4 { ECompressType::No };
};
static_assert(sizeof(FPackedTokenEntryData) == 32, "FPackedTokenEntryData size is not 32 bytes");

/**
 * Token data structure represented by each entry object
 */
struct FPackedTokenEntry
{
	FPackedTokenEntryData Data;
	FPathHash			  PathHash; // RelativeToMountingPoint

	bool		Serialize(FByteBuffer& Buffer) const;
	bool		Deserialize(const FConstByteBuffer& Buffer);
	std::string ToJsonString() const;
};
HLVM_INLINE_VAR constexpr size_t FPackedTokenEntry_SerializedSize = sizeof(FPackedTokenEntry);
static_assert(FPackedTokenEntry_SerializedSize == 40, "FPackedTokenEntry size is not 40 bytes");
// bool							 SerializeTo(const FPackedTokenEntry& Data, FByteBuffer& Buffer);
// bool							 SerializeFrom(FPackedTokenEntry& Data, const FConstByteBuffer& Buffer);
// std::string						 ToJson(const FPackedTokenEntry& Data);

/**
 * Debug data that will serialized to json for debugging propose
 */
struct FPackedTokenEntryWithPath
{
	FPackedTokenEntry Entry;
	std::string		  Path;

	std::string ToJsonString() const;
};
// std::string ToJson(const FPackedTokenEntryWithPath& Data);
