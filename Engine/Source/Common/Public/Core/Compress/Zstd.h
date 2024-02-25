/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "CompressDefinition.h"

#include <string.h>
#include <zstd.h>

// TODO : finish this
class Zstd
{
public:
	static std::string compress(const std::string data, int compress_level)
	{

		size_t est_compress_size = ZSTD_compressBound(data.size());

		std::string comp_buffer{};
		comp_buffer.resize(est_compress_size);

		auto compress_size =
			ZSTD_compress(comp_buffer.data(), est_compress_size, data.data(),
				data.size(), compress_level);

		comp_buffer.resize(compress_size);
		comp_buffer.shrink_to_fit();

		return comp_buffer;
	}

	static std::string decompress(std::string data)
	{

		auto const est_decomp_size =
			ZSTD_getFrameContentSize(data.data(), data.size());

		// TODO : Handle
		// ZSTD_CONTENTSIZE_UNKNOWN, ZSTD_CONTENTSIZE_ERROR, and 0

		std::string decomp_buffer{};
		decomp_buffer.resize(est_decomp_size);

		size_t const decomp_size = ZSTD_decompress(
			decomp_buffer.data(), est_decomp_size, data.data(), data.size());

		decomp_buffer.resize(decomp_size);
		decomp_buffer.shrink_to_fit();
		return decomp_buffer;
	}

	static std::string& buff_compress(const std::string data, std::string& buffer,
		int compress_level)
	{
		size_t est_compress_size = ZSTD_compressBound(data.size());

		buffer.resize(est_compress_size);

		auto compress_size = ZSTD_compress(buffer.data(), est_compress_size,
			data.data(), data.size(), compress_level);

		buffer.resize(compress_size);
		buffer.shrink_to_fit();

		return buffer;
	}

	static std::string& buff_decompress(const std::string& data,
		std::string&									   buffer)
	{
		auto const est_decomp_size =
			ZSTD_getFrameContentSize(data.data(), data.size());

		buffer.resize(est_decomp_size);

		size_t const decomp_size = ZSTD_decompress(
			buffer.data(), est_decomp_size, data.data(), data.size());

		buffer.resize(decomp_size);
		buffer.shrink_to_fit();
		return buffer;
	}
};
