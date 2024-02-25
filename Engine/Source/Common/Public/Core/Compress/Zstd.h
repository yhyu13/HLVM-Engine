/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "CompressDefinition.h"
#include "Core/Container/ContainerDefinition.h"
#include "Core/Assert.h"

#include <zstd.h>

// TODO : finish this
class FZstd
{
public:
	HLVM_STATIC_FUNC TVector<std::byte> Compress(const std::span<std::byte>& data, int compress_level = 1, bool bShrinkOutputBuffer = false);
	HLVM_STATIC_FUNC TVector<std::byte> Decompress(const std::span<std::byte>& data, bool bShrinkOutputBuffer = false);

	// TODO
	//
	//	static std::string& buff_compress(const std::string data, std::string& buffer,
	//		int compress_level)
	//	{
	//		size_t est_compress_size = ZSTD_compressBound(data.size());
	//
	//		buffer.resize(est_compress_size);
	//
	//		auto compress_size = ZSTD_compress(buffer.data(), est_compress_size,
	//			data.data(), data.size(), compress_level);
	//
	//		buffer.resize(compress_size);
	//		buffer.shrink_to_fit();
	//
	//		return buffer;
	//	}
	//
	//	static std::string& buff_decompress(const std::string& data,
	//		std::string&									   buffer)
	//	{
	//		auto const est_decomp_size =
	//			ZSTD_getFrameContentSize(data.data(), data.size());
	//
	//		buffer.resize(est_decomp_size);
	//
	//		size_t const decomp_size = ZSTD_decompress(
	//			buffer.data(), est_decomp_size, data.data(), data.size());
	//
	//		buffer.resize(decomp_size);
	//		buffer.shrink_to_fit();
	//		return buffer;
	//	}
};
