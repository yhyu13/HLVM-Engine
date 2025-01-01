/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "CompressDefinition.h"
#include "Core/Container/ContainerDefinition.h"

#include <zstd.h>
#include <span>

class FZstd
{
public:
	HLVM_NODISCARD HLVM_STATIC_FUNC TVector<TBYTE> Compress(const FConstByteBuffer& data, int compress_level = 1, bool bShrink = HLVM_CONTAINER_SHRINK);
	HLVM_NODISCARD HLVM_STATIC_FUNC TVector<TBYTE> Decompress(const FConstByteBuffer& data, bool bShrink = HLVM_CONTAINER_SHRINK);
};
