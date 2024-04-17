/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Compress/Zstd.h"
#include "Core/Assert.h"
#include "Utility/ScopedTimer.h"

DECLARE_LOG_CATEGORY(LogZstd)

HLVM_NODISCARD TVector<TBYTE> FZstd::Compress(const FConstByteBuffer& data, int compress_level, bool bShrink)
{
	HLVM_SCOPED_TIMER_LOG(FString::Format(TXT("Zstd compress size {} level {}"), data.size(), compress_level));

	size_t est_compress_size = ZSTD_compressBound(data.size());
	HLVM_ENSURE(ZSTD_isError(est_compress_size) == 0, TXT("ZSTD_compressBound = {}, ErrMsg: {}"),
		est_compress_size, TO_TCHAR_STR(ZSTD_getErrorName(est_compress_size)));

	TVector<TBYTE> comp_buffer;
	comp_buffer.resize(est_compress_size);
	auto compress_size = ZSTD_compress(comp_buffer.data(), est_compress_size, data.data(), data.size(), compress_level);
	comp_buffer.resize(compress_size);

	if (bShrink)
	{
		comp_buffer.shrink_to_fit();
	}

	HLVM_LOG(LogZstd, trace, TXT("Zstd compress size {} level {}, est_compress_size {} compress_size {}"),
		data.size(), compress_level, est_compress_size, compress_size);

	return comp_buffer;
}

HLVM_NODISCARD TVector<TBYTE> FZstd::Decompress(const FConstByteBuffer& data, bool bShrink)
{
	HLVM_SCOPED_TIMER_LOG(FString::Format(TXT("Zstd decompress size {}"), data.size()));

	auto const est_decomp_size = ZSTD_getFrameContentSize(data.data(), data.size());
	HLVM_ENSURE(est_decomp_size != ZSTD_CONTENTSIZE_UNKNOWN, TXT("ZSTD_getFrameContentSize = {}, ErrMsg: {}"),
		ZSTD_CONTENTSIZE_UNKNOWN, TXT("it's necessary to use streaming mode to decompress data"));
	HLVM_ENSURE(est_decomp_size != ZSTD_CONTENTSIZE_ERROR, TXT("ZSTD_getFrameContentSize = {}, ErrMsg: {}"),
		ZSTD_CONTENTSIZE_ERROR, TXT("an error occurred"));

	TVector<TBYTE> decomp_buffer;
	decomp_buffer.resize(est_decomp_size);
	size_t const decomp_size = ZSTD_decompress(decomp_buffer.data(), est_decomp_size, data.data(), data.size());
	decomp_buffer.resize(decomp_size);

	if (bShrink)
	{
		decomp_buffer.shrink_to_fit();
	}

	HLVM_LOG(LogZstd, trace, TXT("Zstd decompress size {}, est_decomp_size {} decomp_size {}"),
		data.size(), est_decomp_size, decomp_size);

	return decomp_buffer;
}
