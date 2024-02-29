/**
 * Copyright (c) 2024. MIT License. All rights reserved.
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
	EEncryptType  EncryptType{ EEncryptType::Unkown };
	ECompressType CompressType{ ECompressType::Unkown };
};

/**
 * Token data structure represented by each entry object
 */
struct FPackedTokenEntry
{
	FPackedTokenEntryData Data;
	FPathHash			  PathHash; // RelativeToMountingPoint
};
HLVM_INLINE_VAR constexpr size_t FPackedTokenEntry_SerializedSize = sizeof(FPackedTokenEntry);
bool							 GetSerialized(const FPackedTokenEntry& Data, std::span<std::byte>& Buffer);
bool							 SetSerialized(FPackedTokenEntry& Data, const std::span<const std::byte>& Buffer);

/**
 * Debug data that will serialized to json for debugging propose
 */
struct FPackedTokenEntry_Debug
{
	FPackedTokenEntry Entry;
	std::string		  Path;
};
std::string ToJson(const FPackedTokenEntry_Debug& Data);
