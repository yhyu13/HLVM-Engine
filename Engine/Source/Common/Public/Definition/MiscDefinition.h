/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#define TOKENPASTE(x, y) x##y
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)
#define PADDING(size) TBYTE TOKENPASTE2(__padding_, __LINE__)[size]
#define TOKENPASTE2LINE(varname) TOKENPASTE2(varname, __LINE__)

// bool x:1; May not be reliable for all compilers (e.g. msvc, gnu, clang), so disable,
// padding for bool may generally be a bad idea
#define BIT_FLAG(x) bool x

#define INVALID_INDEX_SIZE_T std::numeric_limits<size_t>::max()
#define INVALID_INDEX_UINT32 std::numeric_limits<uint32_t>::max()

#define DEPRECATED(Version, Message) [[deprecated(Message)]]
