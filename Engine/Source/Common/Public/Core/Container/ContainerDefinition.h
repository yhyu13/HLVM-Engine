/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"

#include <boost/container/vector.hpp>
#include <boost/container/small_vector.hpp>

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

template <typename T, size_t N>
using TSVector = boost::container::small_vector<T, N>;

template <typename T>
using TSVector32 = boost::container::small_vector<T, 32>;

template <typename T>
using TVector = boost::container::vector<T>;

template <typename Key, typename Value>
using TSMap = phmap::flat_hash_map<Key, Value>;

template <typename Key, typename Value>
using TMap = phmap::node_hash_map<Key, Value>;
