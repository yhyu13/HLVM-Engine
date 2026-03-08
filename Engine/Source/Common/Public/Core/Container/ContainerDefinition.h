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
#include <boost/range/algorithm/find.hpp>

#include "Core/Mallocator/PMR.h"
#include "Core/Assert.h"

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

// Auto shrink
#define HLVM_CONTAINER_SHRINK 0
// Num() shoue return uint64
#define HLVM_CONTAINER_NUM_64 1

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

	TUINT32 Num32() const
	{
		HLVM_ASSERT(this->size() <= S_C(size_t, TUINT32_MAX));
		return S_C(TUINT32, this->size());
	}
#if HLVM_CONTAINER_NUM_64
	TUINT64 Num() const
	{
		return this->size();
	}
#else
	TUINT32 Num() const
	{
		return this->Num32();
	}
#endif

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

	TUINT32 Num32() const
	{
		HLVM_ASSERT(this->size() <= S_C(size_t, TUINT32_MAX));
		return S_C(TUINT32, this->size());
	}
#if HLVM_CONTAINER_NUM_64
	TUINT64 Num() const
	{
		return this->size();
	}
#else
	TUINT32 Num() const
	{
		return this->Num32();
	}
#endif

	TUINT32 SetNum(TUINT32 Num)
	{
		this->resize(Num);
		return this->Num();
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

	/**
	 * @brief Equivalent to std::vector::push_back(const T&)
	 * @return The index of the added element
	 */
	TUINT32 Add(const T& Value)
	{
		this->push_back(Value);
		HLVM_ASSERT(this->size() <= S_C(size_t, TUINT32_MAX));
		return this->size() - 1;
	}

	/**
	 * @brief Equivalent to std::vector::push_back(T&&)
	 * @return The index of the added element
	 */
	TUINT32 Add(T&& Value)
	{
		this->push_back(Value);
		HLVM_ASSERT(this->size() <= S_C(size_t, TUINT32_MAX));
		return this->size() - 1;
	}

	/**
	 * @brief Equivalent to std::vector::reserve()
	 * @return The capacity of the container after the call
	 */
	TSIZE Reserve(TSIZE NewCapacity)
	{
		this->reserve(NewCapacity);
		return this->capacity();
	}

	/**
	 * @brief Equivalent to std::vector::resize()
	 * @return The size of the container after the call
	 */
	TSIZE AddDefaulted(TSIZE NumDefaulted)
	{
		if (NumDefaulted > 0)
		{
			this->resize(this->size() + NumDefaulted);
		}
		return this->size();
	}

	T& AddDefaulted_GetRef()
	{
		this->emplace_back();
		return this->back();
	}

	T* LastData() const
	{
		if (this->size() > 0)
		{
			return C_C(T*, &(this->back()));
		}
		else
		{
			return nullptr;
		}
	}

	void Swap(TSIZE Index1, TSIZE Index2)
	{
		if (Index1 < this->size() && Index2 < this->size())
		{
			std::iter_swap(this->begin() + Index1, this->begin() + Index2);
		}
		else
		{
			HLVM_ASSERT_F(false, TXT("Invalid index, Index1 = {}, Index2 = {}"), Index1, Index2);
		}
	}

	bool IsEmpty() const
	{
		return this->size() == 0;
	}

	void Empty(TSIZE NewCapacity = 0)
	{
		this->clear();
		if (this->capacity() < NewCapacity)
		{
			this->shrink_to_fit();
		}
		this->reserve(NewCapacity);
		HLVM_ASSERT(this->capacity() >= NewCapacity);
	}

	void Reset(TSIZE NewCapacity = 0)
	{
		this->clear();
		this->reserve(NewCapacity);
		HLVM_ASSERT(this->capacity() >= NewCapacity);
	}

	T& Last()
	{
		HLVM_ASSERT(this->size() > 0);
		return this->back();
	}

	const T& Last() const
	{
		HLVM_ASSERT(this->size() > 0);
		return this->back();
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

	TUINT32 Num32() const
	{
		HLVM_ASSERT(this->size() <= S_C(size_t, TUINT32_MAX));
		return S_C(TUINT32, this->size());
	}
#if HLVM_CONTAINER_NUM_64
	TUINT64 Num() const
	{
		return this->size();
	}
#else
	TUINT32 Num() const
	{
		return this->Num32();
	}
#endif

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

	void Empty(TSIZE NewCapacity = 0)
	{
		this->clear();
		if (this->capacity() < NewCapacity)
		{
			this->shrink_to_fit();
		}
		this->reserve(NewCapacity);
		HLVM_ASSERT(this->capacity() >= NewCapacity);
	}

	void Reset(TSIZE NewCapacity = 0)
	{
		this->clear();
		this->reserve(NewCapacity);
		HLVM_ASSERT(this->capacity() >= NewCapacity);
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

	TUINT32 Num32() const
	{
		HLVM_ASSERT(this->size() <= S_C(size_t, TUINT32_MAX));
		return S_C(TUINT32, this->size());
	}
#if HLVM_CONTAINER_NUM_64
	TUINT64 Num() const
	{
		return this->size();
	}
#else
	TUINT32 Num() const
	{
		return this->Num32();
	}
#endif

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
		return std::addressof(Iter->second);
	}

	const Value* Find(const Key& key) const
	{
		auto Iter = this->find(key);
		if (Iter == this->end())
		{
			return nullptr;
		}
		return std::addressof(Iter->second);
	}

	bool Contains(const Key& key) const
	{
		return this->find(key) != this->end();
	}

	// Add
	Value* Add(const Key& key, const Value& value)
	{
		auto Iter = this->find(key);
		if (Iter == this->end())
		{
			auto result = this->insert(std::pair<Key, Value>{ key, value });
			Iter = result.first;
			HLVM_ASSERT(result.second);
		}
		else
		{
			Iter->second = value;
		}
		return std::addressof(Iter->second);
	}

	// Add move
	Value* Add(const Key& key, Value&& value)
	{
		auto Iter = this->find(key);
		if (Iter == this->end())
		{
			auto result = this->insert(MoveTemp(std::pair<Key, Value>{ key, MoveTemp(value) }));
			Iter = result.first;
			HLVM_ASSERT(result.second);
		}
		else
		{
			Iter->second = MoveTemp(value);
		}
		return std::addressof(Iter->second);
	}

	void Remove(const Key& key)
	{
		this->erase(key);
	}

	void Empty(TSIZE NewCapacity = 0)
	{
		// Node hash map has no shrink_to_fit(), so we don't need to call it
		// Equivalent to Rest
		Reset(NewCapacity);
	}

	void Reset(TSIZE NewCapacity = 0)
	{
		this->clear();
		this->reserve(NewCapacity);
		HLVM_ASSERT(this->capacity() >= NewCapacity);
	}
};

template <typename Key, typename Value, typename Allocator = std::allocator<std::pair<Key, Value>>>
class TMap : public phmap::node_hash_map<Key, Value, std::hash<Key>, std::equal_to<Key>, Allocator>
{
public:
	using phmap::node_hash_map<Key, Value, std::hash<Key>, std::equal_to<Key>, Allocator>::node_hash_map;

	TUINT32 Num32() const
	{
		HLVM_ASSERT(this->size() <= S_C(size_t, TUINT32_MAX));
		return S_C(TUINT32, this->size());
	}
#if HLVM_CONTAINER_NUM_64
	TUINT64 Num() const
	{
		return this->size();
	}
#else
	TUINT32 Num() const
	{
		return this->Num32();
	}
#endif

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
		return std::addressof(Iter->second);
	}

	const Value* Find(const Key& key) const
	{
		auto Iter = this->find(key);
		if (Iter == this->end())
		{
			return nullptr;
		}
		return std::addressof(Iter->second);
	}

	bool Contains(const Key& key) const
	{
		return this->find(key) != this->end();
	}

	// Add
	Value* Add(const Key& key, const Value& value)
	{
		auto Iter = this->find(key);
		if (Iter == this->end())
		{
			auto result = this->insert(std::pair<Key, Value>{ key, value });
			Iter = result.first;
			HLVM_ASSERT(result.second);
		}
		else
		{
			Iter->second = value;
		}
		return std::addressof(Iter->second);
	}

	// Add move
	Value* Add(const Key& key, Value&& value)
	{
		auto Iter = this->find(key);
		if (Iter == this->end())
		{
			auto result = this->insert(MoveTemp(std::pair<Key, Value>{ key, MoveTemp(value) }));
			Iter = result.first;
			HLVM_ASSERT(result.second);
		}
		else
		{
			Iter->second = MoveTemp(value);
		}
		return std::addressof(Iter->second);
	}

	void Remove(const Key& key)
	{
		this->erase(key);
	}

	void Empty(TSIZE NewCapacity = 0)
	{
		// Node hash map has no shrink_to_fit(), so we don't need to call it
		// Equivalent to Rest
		Reset(NewCapacity);
	}

	void Reset(TSIZE NewCapacity = 0)
	{
		this->clear();
		this->reserve(NewCapacity);
		HLVM_ASSERT(this->capacity() >= NewCapacity);
	}
};

template <typename T, typename Allocator = std::allocator<T>>
using TSetSmall = phmap::flat_hash_set<T, std::hash<T>, std::equal_to<T>, Allocator>;

template <typename T, typename Allocator = std::allocator<T>>
using TSet = phmap::node_hash_set<T, std::hash<T>, std::equal_to<T>, Allocator>;

using FByteVector = TVector<TBYTE>;
using FByteBuffer = std::span<TBYTE>;
using FConstByteBuffer = std::span<const TBYTE>;

template <typename T, typename Allocator = boost::container::new_allocator<T>>
using TRingBuffer = boost::circular_buffer<T, Allocator>;

template <typename T>
using TArray = TVector<T>;

template <typename T>
using TArrayView = TVectorView<T>;
