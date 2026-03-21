/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"

/**
 * zstd options explanation
 * https://github.com/facebook/zstd
 * https://github.com/andreiamatuni/zstdpp/blob/master/zstdpp.hpp
 */
HLVM_ENUM(ECompressType, TUINT8,
	ZSTD_1,
	ZSTD_4,
	No);


