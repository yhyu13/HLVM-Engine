/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "ParallelDefinition.h"

#ifndef HLVM_ATOMIC_LOCK_ENABLE_PADDING
	#define HLVM_ATOMIC_LOCK_ENABLE_PADDING 0
#endif

#if !HLVM_BUILD_RELEASE
	#define HLVM_DEADLOCK_TIMER 1 // Debug break on potential dead lock
#else
	#define HLVM_DEADLOCK_TIMER 0
#endif // !HLVM_BUILD_RELEASE

class FAtomicLockGuard
{
public:
	NOCOPYMOVE(FAtomicLockGuard)
	FAtomicLockGuard() = delete;
	explicit FAtomicLockGuard(std::atomic_flag& flag) noexcept(!HLVM_DEADLOCK_TIMER);
	~FAtomicLockGuard() noexcept;

private:
	std::atomic_flag* mLock;
};

template <typename T>
concept Lockable = requires(T t) {
	{
		t.Lock()
	} -> std::same_as<void>;
	{
		t.Unlock()
	} -> std::same_as<void>;
};

template <Lockable T>
class TAtomicLockGuard
{
public:
	NOCOPYMOVE(TAtomicLockGuard)
	TAtomicLockGuard() = delete;
	explicit TAtomicLockGuard(T& Flag) noexcept(!HLVM_DEADLOCK_TIMER)
		: mLock(&Flag)
	{
		mLock->Lock();
	}
	explicit TAtomicLockGuard(std::optional<T>& Flag) noexcept(!HLVM_DEADLOCK_TIMER)
		: mLock(Flag.has_value() ? &Flag.value() : nullptr)
	{
		if (mLock)
		{
			mLock->Lock();
		}
	}
	~TAtomicLockGuard() noexcept
	{
		if (mLock)
		{
			mLock->Unlock();
		}
	}

private:
	T* mLock;
};

#define ATOMIC_LOCK_GUARD(x)                                                                           \
	TAtomicLockGuard<typename TOptionalRemoved<typename TReferenceRemoved<decltype((x))>::Type>::Type> \
		__lock_guard((x));                                                                             \
	ATOMIC_THREAD_FENCE()

/**
 * @class FAtomicFlagStatic
 * @brief 一个静态原子标志类
 */
class FAtomicFlagStatic
{
public:
#define LOCK_GUARD_S()                        \
	FAtomicLockGuard __lock_guard_s(sc_flag); \
	ATOMIC_THREAD_FENCE()

	static void LockS() noexcept(!HLVM_DEADLOCK_TIMER);
	static void UnlockS() noexcept;

protected:
	HLVM_CACHE_ALIGN inline static std::atomic_flag sc_flag{ 0 };
};

/**
 * @class FAtomicFlagNI
 * @brief 一个非实例原子标志类
 */
class FAtomicFlagNI
{
public:
	NOINSTANT(FAtomicFlagNI)
#define LOCK_GUARD_NI()                        \
	FAtomicLockGuard __lock_guard_ni(ni_flag); \
	ATOMIC_THREAD_FENCE()

	static void LockNI() noexcept(!HLVM_DEADLOCK_TIMER);
	static void UnlockNI() noexcept;

protected:
	HLVM_CACHE_ALIGN inline static std::atomic_flag ni_flag{ 0 };
};

/**
 * @class FAtomicFlagNC
 * @brief 一个具有非复制的原子标志类
 */
class FAtomicFlagNC
{
public:
	NOCOPYMOVE(FAtomicFlagNC)

#define LOCK_GUARD_NC()                        \
	FAtomicLockGuard __lock_guard_nc(nc_flag); \
	ATOMIC_THREAD_FENCE()

	FAtomicFlagNC() = default;

	void LockNC() const noexcept(!HLVM_DEADLOCK_TIMER);
	void UnlockNC() const noexcept;

protected:
	// Prevent delete by this pointer type, this way compiler would not allow it
	~FAtomicFlagNC() noexcept = default;

	mutable std::atomic_flag nc_flag{ 0 };

private:
#if HLVM_ATOMIC_LOCK_ENABLE_PADDING
	PADDING(HLVM_PLATFORM_CACHE_LINE - sizeof(std::atomic_flag));
#endif
};

/**
 * @class FAtomicFlagNC
 * @brief 一个通常的的原子标志类
 */
class FAtomicFlag
{
public:
#define LOCK_GUARD()                       \
	FAtomicLockGuard __lock_guard_(mFlag); \
	ATOMIC_THREAD_FENCE()

	FAtomicFlag() noexcept = default;
	~FAtomicFlag() noexcept = default;

	FAtomicFlag(const FAtomicFlag&) noexcept
		: mFlag{ 0 }
	{
		// Trivial
	}

	FAtomicFlag(FAtomicFlag&&) noexcept
		: mFlag{ 0 }
	{
		// Trivial
	}

	FAtomicFlag& operator=(const FAtomicFlag&) noexcept
	{
		// Trivial
		return *this;
	}

	FAtomicFlag& operator=(FAtomicFlag&&) noexcept
	{
		// Trivial
		return *this;
	}

	void Lock() const noexcept(!HLVM_DEADLOCK_TIMER);
	void Unlock() const noexcept;

protected:
	mutable std::atomic_flag mFlag{ 0 };

private:
#if HLVM_ATOMIC_LOCK_ENABLE_PADDING
	PADDING(HLVM_PLATFORM_CACHE_LINE - sizeof(std::atomic_flag));
#endif
};

/**
 * @class FRecursiveAtomicFlag
 * @brief 一个允许同一线程内无需竞争锁的原子标志类
 */
class FRecursiveAtomicFlag
{
public:
#define LOCK_GUARD_RECURSIVE()                                                                 \
	TConstructorTrick __lock_guard_([this]() { this->Lock(); }, [this]() { this->Unlock(); }); \
	ATOMIC_THREAD_FENCE()

	FRecursiveAtomicFlag() noexcept = default;
	~FRecursiveAtomicFlag() noexcept = default;

	FRecursiveAtomicFlag(const FRecursiveAtomicFlag&) noexcept
		: mFlag{ 0 }
	{
		// Trivial
	}

	FRecursiveAtomicFlag(FRecursiveAtomicFlag&&) noexcept
		: mFlag{ 0 }
	{
		// Trivial
	}

	FRecursiveAtomicFlag& operator=(const FRecursiveAtomicFlag&) noexcept
	{
		// Trivial
		return *this;
	}
	FRecursiveAtomicFlag& operator=(FRecursiveAtomicFlag&&) noexcept
	{
		// Trivial
		return *this;
	}

	void Lock() const noexcept(!HLVM_DEADLOCK_TIMER);
	void Unlock() const noexcept;

protected:
	mutable std::atomic_flag		  mFlag{ 0 };
	mutable std::thread::id			  mOwner;
	mutable std::atomic_uint_fast32_t mCount = 0;

private:
#if HLVM_ATOMIC_LOCK_ENABLE_PADDING
	PADDING(HLVM_PLATFORM_CACHE_LINE - sizeof(std::atomic_flag));
#endif
};

#undef HLVM_ATOMIC_LOCK_ENABLE_PADDING
