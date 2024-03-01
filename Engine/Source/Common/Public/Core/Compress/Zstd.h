/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "CompressDefinition.h"
#include "Core/Container/ContainerDefinition.h"

#include <zstd.h>
#include <span>

class FZstd
{
public:
	HLVM_STATIC_FUNC TVector<std::byte> Compress(const FByteBuffer& data, int compress_level = 1, bool bShrinkOutputBuffer = false);
	HLVM_STATIC_FUNC TVector<std::byte> Decompress(const FByteBuffer& data, bool bShrinkOutputBuffer = false);
};
