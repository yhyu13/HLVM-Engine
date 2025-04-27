/**
 * Copyright (c) 2025. MIT License. All rights reserved.
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

template <typename T>
class TVectorView : public std::span<T>
{
public:
	using std::span<T>::span;

	TUINT32 Num() const
	{
		HLVM_ASSERT(this->size() <= S_C(size_t, TUINT32_MAX));
		return S_C(TUINT32, this->size());
	}

	T* GetData() const
	{
		return this->data();
	}

	const T* GetDataConst() const
	{
		return this->data();
	}
};

template <typename T, typename Allocator = boost::container::new_allocator<T>>
class TVector : public boost::container::vector<T, Allocator>
{
public:
	// Inheriting constructors
	// https://stackoverflow.com/a/434784
	using boost::container::vector<T, Allocator>::vector;

	TUINT32 Num() const
	{
		HLVM_ASSERT(this->size() <= S_C(size_t, TUINT32_MAX));
		return S_C(TUINT32, this->size());
	}

	TSIZE Size() const
	{
		return this->size();
	}

	TSIZE NumBytes() const
	{
		return this->size() * sizeof(T);
	}

	T* GetData() const
	{
		return C_C(T*, this->data());
	}

	const T* GetDataConst() const
	{
		return this->data();
	}

	TSIZE Add(const T& Value)
	{
		this->push_back(Value);
		return this->size() - 1;
	}

	TSIZE Add(T&& Value)
	{
		this->push_back(Value);
		return this->size() - 1;
	}

	T* LastData() const
	{
		HLVM_ASSERT(this->size() > 0);
		return C_C(T*, &(this->back()));
	}

	void Swap(TSIZE Index1, TSIZE Index2)
	{
		HLVM_ASSERT(Index1 < this->size() && Index2 < this->size());
		std::iter_swap(this->begin() + Index1, this->begin() + Index2);
	}

	void Empty(TSIZE NewSize = 0)
	{
		this->clear();
		this->resize(NewSize);
	}

	void Reset(TSIZE NewSize = 0)
	{
		TSIZE ReserveSize = this->size();
		if (NewSize != 0)
		{
			ReserveSize = NewSize;
		}
		this->clear();
		this->reserve(ReserveSize);
	}

	operator TVectorView<T>() const
	{
		return TVectorView<T>(this->data(), this->size());
	}
};


template <typename T, std::size_t N>
class TStaticVector : public boost::container::static_vector<T, N>
{
public:
	using boost::container::static_vector<T, N>::static_vector;

	TUINT32 Num() const
	{
		HLVM_ASSERT(this->size() <= S_C(size_t, TUINT32_MAX));
		return S_C(TUINT32, this->size());
	}

	TSIZE Size() const
	{
		return this->size();
	}

	TSIZE NumBytes() const
	{
		return this->size() * sizeof(T);
	}

	T* GetData() const
	{
		return C_C(T*, this->data());
	}

	const T* GetDataConst() const
	{
		return this->data();
	}

	TSIZE Add(const T& Value)
	{
		this->push_back(Value);
		return this->size() - 1;
	}

	TSIZE Add(T&& Value)
	{
		this->push_back(Value);
		return this->size() - 1;
	}

	T* LastData() const
	{
		HLVM_ASSERT(this->size() > 0);
		return C_C(T*, &(this->back()));
	}

	void Swap(TSIZE Index1, TSIZE Index2)
	{
		HLVM_ASSERT(Index1 < this->size() && Index2 < this->size());
		std::iter_swap(this->begin() + Index1, this->begin() + Index2);
	}

	operator TVectorView<T>() const
	{
		return TVectorView<T>(this->data(), this->size());
	}
};

template <typename Key, typename Value, typename Allocator = std::allocator<std::pair<Key, Value>>>
class TMapSmall : public phmap::flat_hash_map<Key, Value, std::hash<Key>, std::equal_to<Key>, Allocator>
{
public:
	using phmap::flat_hash_map<Key, Value, std::hash<Key>, std::equal_to<Key>, Allocator>::flat_hash_map;

	// Num
	TUINT32 Num() const
	{
		HLVM_ASSERT(this->size() <= S_C(size_t, TUINT32_MAX));
		return S_C(TUINT32, this->size());
	}

	// Size
	TSIZE Size() const
	{
		return this->size();
	}

	Value* Find(const Key& key)
	{
		auto Iter = this->find(key);
		if (Iter == this->end())
		{
			return nullptr;
		}
		return &Iter->second;
	}

	const Value* Find(const Key& key) const
	{
		auto Iter = this->find(key);
		if (Iter == this->end())
		{
			return nullptr;
		}
		return &Iter->second;
	}

	// Add
	Value* Add(const Key& key, const Value& value)
	{
		auto Iter = this->find(key);
		if (Iter == this->end())
		{
			Iter = this->insert({key, value}).first;
		}
		else
		{
			Iter->second = value;
		}
		return &Iter->second;
	}

	// Add move
	Value* Add(const Key& key, Value&& value)
	{
		auto Iter = this->find(key);
		if (Iter == this->end())
		{
			Iter = this->insert({key, value}).first;
		}
		else
		{
			Iter->second = value;
		}
		return &Iter->second;
	}
};

template <typename Key, typename Value, typename Allocator = std::allocator<std::pair<Key, Value>>>
class TMap : public phmap::flat_hash_map<Key, Value, std::hash<Key>, std::equal_to<Key>, Allocator>
{
public:
	using phmap::flat_hash_map<Key, Value, std::hash<Key>, std::equal_to<Key>, Allocator>::flat_hash_map;

	// Num
	TUINT32 Num() const
	{
		HLVM_ASSERT(this->size() <= S_C(size_t, TUINT32_MAX));
		return S_C(TUINT32, this->size());
	}

	// Size
	TSIZE Size() const
	{
		return this->size();
	}

	Value* Find(const Key& key)
	{
		auto Iter = this->find(key);
		if (Iter == this->end())
		{
			return nullptr;
		}
		return &Iter->second;
	}

	const Value* Find(const Key& key) const
	{
		auto Iter = this->find(key);
		if (Iter == this->end())
		{
			return nullptr;
		}
		return &Iter->second;
	}

	// Add
	Value* Add(const Key& key, const Value& value)
	{
		auto Iter = this->find(key);
		if (Iter == this->end())
		{
			Iter = this->insert({key, value}).first;
		}
		else
		{
			Iter->second = value;
		}
		return &Iter->second;
	}

	// Add move
	Value* Add(const Key& key, Value&& value)
	{
		auto Iter = this->find(key);
		if (Iter == this->end())
		{
			Iter = this->insert({key, value}).first;
		}
		else
		{
			Iter->second = value;
		}
		return &Iter->second;
	}
};

template <typename T, typename Allocator = std::allocator<T>>
using TSetSmall = phmap::flat_hash_set<T, std::hash<T>, std::equal_to<T>, Allocator>;

template <typename T, typename Allocator = std::allocator<T>>
using TSet = phmap::node_hash_set<T, std::hash<T>, std::equal_to<T>, Allocator>;

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
