/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#define TOKENPASTE(x, y) x##y
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)
#define PADDING(size) TBYTE TOKENPASTE2(__padding_, __LINE__)[size]

#define BIT_FLAG(x) bool x // bool x : 1 // May not be reliable for all compilers, So disable

#define INVALID_INDEX_SIZE_T() std::numeric_limits<size_t>::max()
