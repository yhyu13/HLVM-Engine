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
	HLVM_STATIC_FUNC TVector<std::byte> Compress(const std::span<std::byte>& data, int compress_level = 1, bool bShrinkOutputBuffer = false);
	HLVM_STATIC_FUNC TVector<std::byte> Decompress(const std::span<std::byte>& data, bool bShrinkOutputBuffer = false);
};
