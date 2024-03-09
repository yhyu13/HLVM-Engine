/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"

#include <boost/container/vector.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/algorithm/string/join.hpp>

/**
 * phmap has alot of unconventional warnings, pretty bad code though
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcomma"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#include <parallel_hashmap/phmap.h>
#pragma clang diagnostic pop

#define HLVM_CONTAINER_SHRINK false

template <typename T, size_t N>
using TSmallVector = boost::container::small_vector<T, N>;

template <typename T>
using TSmallVector32 = boost::container::small_vector<T, 32>;

template <typename T>
using TVector = boost::container::vector<T>;

template <typename Key, typename Value>
using TMap = phmap::flat_hash_map<Key, Value>;

template <typename Key, typename Value>
using TStableMap = phmap::node_hash_map<Key, Value>;

#define HLVM_MAP_FIND(map, key)        \
	if (auto iter = (map).find((key)); \
		iter != (map).end())

using FByteVector = TVector<TBYTE>;
using FByteBuffer = std::span<TBYTE>;
using FConstByteBuffer = std::span<const TBYTE>;
#define TO_SPAN(array, size) \
	std::span                \
	{                        \
		(array), size        \
	}
#define TO_CONST_BYTE_BUFFER(vec)                     \
	FConstByteBuffer                                  \
	{                                                 \
		R_C(const TBYTE*, (vec).data()), (vec).size() \
	}
