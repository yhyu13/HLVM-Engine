/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"

#include <span>
#include <boost/container/vector.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/container/static_vector.hpp>

#include "Core/Mallocator/PMR.h"

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

// TODO : set all container growth factor to 1
template <typename T, size_t N, typename Allocator = boost::container::new_allocator<T>>
using TSmallVector = boost::container::small_vector<T, N, Allocator>;

template <typename T, typename Allocator = boost::container::new_allocator<T>>
using TSmallVector32 = boost::container::small_vector<T, 32, Allocator>;

template <typename T, typename Allocator = boost::container::new_allocator<T>>
using TSmallVector64 = boost::container::small_vector<T, 64, Allocator>;

template <typename T, typename Allocator = boost::container::new_allocator<T>>
using TVector = boost::container::vector<T, Allocator>;

template <typename Key, typename Value, typename Allocator = std::allocator<std::pair<Key, Value>>>
using TMap = phmap::flat_hash_map<Key, Value, std::hash<Key>, std::equal_to<Key>, Allocator>;

template <typename Key, typename Value, typename Allocator = std::allocator<std::pair<Key, Value>>>
using TStableMap = phmap::node_hash_map<Key, Value, std::hash<Key>, std::equal_to<Key>, Allocator>;

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

template <typename T, typename Allocator = boost::container::new_allocator<T>>
using TRingBuffer = boost::circular_buffer<T, Allocator>;

template <typename T, std::size_t N>
using TFixedSizeVector = boost::container::static_vector<T, N>;
