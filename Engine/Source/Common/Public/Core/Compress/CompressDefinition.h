/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"

HLVM_ENUM(ECompressType, uint8_t,
	ZSTD_0,
	ZSTD_4,
	No,
	Unkown);

/**
 * https://github.com/facebook/zstd
 * https://github.com/andreiamatuni/zstdpp/blob/master/zstdpp.hpp
 */
