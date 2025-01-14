/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

// A base class for reference counting which supports non-copyable inherited classes.
struct FRefCountable
{
public:
	FRefCountable() = default;
	FRefCountable(const FRefCountable&)
	{
		// Trivial,
		// copy construct would not copy the value of counter as it is a brand new object
	}
	FRefCountable(FRefCountable&& Other)
	{
		mCounter.store(Other.mCounter.load(std::memory_order_relaxed), std::memory_order_relaxed);
	}
	FRefCountable& operator=(const FRefCountable&)
	{
		// Trivial,
		// copy construct would not copy the value of counter as it is a brand new object
		return *this;
	}
	FRefCountable& operator=(FRefCountable&& Other)
	{
		mCounter.store(Other.mCounter.load(std::memory_order_relaxed), std::memory_order_relaxed);
		return *this;
	}

	HLVM_INLINE_FUNC void IncrementRef() const noexcept
	{
		mCounter.fetch_add(1, std::memory_order_relaxed);
	}

	HLVM_INLINE_FUNC bool DecrementRef() const noexcept
	{
		return mCounter.fetch_sub(1, std::memory_order_relaxed) > 1;
	}

	HLVM_INLINE_FUNC size_t RefCount() const noexcept
	{
		return mCounter.load(std::memory_order_relaxed);
	}

private:
	mutable std::atomic_uint_fast32_t mCounter{ 0 };
};

// Concept for reference countable classes
template <typename T>
concept CRefCountable = requires(T&& t) {
	{
		t.IncrementRef()
	};
} && requires(T&& t) {
	{
		t.DecrementRef()
	} -> std::convertible_to<bool>;
};
