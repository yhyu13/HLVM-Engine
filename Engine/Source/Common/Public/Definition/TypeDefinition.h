/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <chrono>

#define TBYTE std::byte
static_assert(sizeof(TBYTE) == sizeof(char), "TBYTE is not char in size");

#define TUINT8 std::uint8_t
static_assert(sizeof(TUINT8) == sizeof(char), "uint8_t is not char in size");
#define TUINT8_MIN std::numeric_limits<TUINT8>::min()
#define TUINT8_MAX std::numeric_limits<TUINT8>::max()

#define TUINT16 std::uint16_t
static_assert(sizeof(TUINT16) == 2 * sizeof(char), "uint16_t is not 2 char in size");
#define TUINT16_MIN std::numeric_limits<TUINT16>::min()
#define TUINT16_MAX std::numeric_limits<TUINT16>::max()

#define TUINT32 std::uint32_t
static_assert(sizeof(TUINT32) == 4 * sizeof(char), "uint32_t is not 4 char in size");
#define TUINT32_MIN std::numeric_limits<TUINT32>::min()
#define TUINT32_MAX std::numeric_limits<TUINT32>::max()

#define TUINT64 std::uint64_t
static_assert(sizeof(TUINT64) == 8 * sizeof(char), "uint64_t is not 8 char in size");
#define TUINT64_MIN std::numeric_limits<TUINT64>::min()
#define TUINT64_MAX std::numeric_limits<TUINT64>::max()

#define TUINT TUINT64
#define TSIZE TUINT64

#define TINT8 std::int8_t
static_assert(sizeof(TINT8) == sizeof(char), "int8_t is not char in size");
#define TINT8_MIN std::numeric_limits<TINT8>::min()
#define TINT8_MAX std::numeric_limits<TINT8>::max()

#define TINT16 std::int16_t
static_assert(sizeof(TINT16) == 2 * sizeof(char), "int16_t is not 2 char in size");
#define TINT16_MIN std::numeric_limits<TINT16>::min()
#define TINT16_MAX std::numeric_limits<TINT16>::max()

#define TINT32 std::int32_t
static_assert(sizeof(TINT32) == 4 * sizeof(char), "int32_t is not 4 char in size");
#define TINT32_MIN std::numeric_limits<TINT32>::min()
#define TINT32_MAX std::numeric_limits<TINT32>::max()

#define TINT64 std::int64_t
static_assert(sizeof(TINT64) == 8 * sizeof(char), "int64_t is not 8 char in size");
#define TINT64_MIN std::numeric_limits<TINT64>::min()
#define TINT64_MAX std::numeric_limits<TINT64>::max()

#define TINT TINT64

#define TFP32 std::float_t
static_assert(sizeof(TFP32) == 4 * sizeof(char), "float_t is not 4 char in size");
#define TFP32_MIN std::numeric_limits<TFP32>::min()
#define TFP32_MAX std::numeric_limits<TFP32>::max()

#define TFP64 std::double_t
static_assert(sizeof(TFP64) == 8 * sizeof(char), "double_t is not 8 char in size");
#define TFP64_MIN std::numeric_limits<TFP64>::min()
#define TFP64_MAX std::numeric_limits<TFP64>::max()

#define TFLOAT TFP64

#define TTimePoint std::chrono::steady_clock::time_point
static_assert(sizeof(TTimePoint) == 8 * sizeof(char), "time_point is not 8 char in size");
