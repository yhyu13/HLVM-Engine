/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "ParallelDefinition.h"
#include "Platform/GenericPlatformAtomicPointer.h"

#ifndef HLVM_ATOMIC_LOCK_ENABLE_PADDING
	#define HLVM_ATOMIC_LOCK_ENABLE_PADDING 1
#endif

#if HLVM_ATOMIC_LOCK_ENABLE_PADDING
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wunused-private-field"
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
	HLVM_ATOMIC_THREAD_FENCE()

/**
 * @class FAtomicFlagStatic
 * @brief 一个静态原子标志类
 */
class FAtomicFlagStatic
{
public:
#define LOCK_GUARD_S()                        \
	FAtomicLockGuard __lock_guard_s(sc_flag); \
	HLVM_ATOMIC_THREAD_FENCE()

	static void Lock() noexcept(!HLVM_DEADLOCK_TIMER);
	static void Unlock() noexcept;

protected:
	HLVM_CACHE_ALIGN HLVM_INLINE_VAR HLVM_STATIC_VAR std::atomic_flag sc_flag{ 0 };
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
	HLVM_ATOMIC_THREAD_FENCE()

	static void Lock() noexcept(!HLVM_DEADLOCK_TIMER);
	static void Unlock() noexcept;

protected:
	HLVM_CACHE_ALIGN HLVM_INLINE_VAR HLVM_STATIC_VAR std::atomic_flag ni_flag{ 0 };
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
	HLVM_ATOMIC_THREAD_FENCE()

	FAtomicFlagNC() = default;

	void Lock() const noexcept(!HLVM_DEADLOCK_TIMER);
	void Unlock() const noexcept;

protected:
	mutable std::atomic_flag nc_flag{ 0 };

private:
#if HLVM_ATOMIC_LOCK_ENABLE_PADDING
	PADDING(HLVM_CACHE_LINE_SIZE - sizeof(std::atomic_flag));
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
	HLVM_ATOMIC_THREAD_FENCE()

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
	PADDING(HLVM_CACHE_LINE_SIZE - sizeof(std::atomic_flag));
#endif
};

/**
 * @class FRecursiveAtomicFlag
 * @brief 一个允许同一线程内无需竞争锁的原子标志类
 */
class FRecursiveAtomicFlag
{
public:
#define LOCK_GUARD_RECURSIVE()                                                                                                                             \
	TScopedVariable<std::function<void()>, std::function<void()>> __lock_guard_([this]() -> void { this->Lock(); }, [this]() -> void { this->Unlock(); }); \
	HLVM_ATOMIC_THREAD_FENCE()

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
	PADDING(HLVM_CACHE_LINE_SIZE - sizeof(std::atomic_flag) - sizeof(std::thread::id) - sizeof(std::atomic_uint_fast32_t));
#endif
};

/**
 * Define 2 rival groups, threads from only one rival group could enter the critical section
 *  This is useful for the parallelism of excluding reader-writer from each other.
 */
class FRWRivalLock
{
public:
	enum Group
	{
		Read = 0,
		Write = 1,
		NUM_GROUPS = 2
	};

	NOCOPYMOVE(FRWRivalLock)
	FRWRivalLock() = default;

	void Lock(int group) const noexcept(!HLVM_DEADLOCK_TIMER);
	void Unlock() const noexcept;

private:
	HLVM_CACHE_ALIGN mutable TAtomicPointer<Group*> mCurrentGroupPtr{ nullptr };
	Group											mGroups[NUM_GROUPS]{ Read, Write };
	mutable std::atomic_uint_fast32_t				mProgramCounter{ 0 };
};

// Conditionally apply rival lock. If not enabled, rival lock would not take actual effect.
template <typename RivalGroupType>
struct RivialLockGuardCond
{
	NOCOPYMOVE(RivialLockGuardCond)
	RivialLockGuardCond() = delete;

	explicit RivialLockGuardCond(RivalGroupType& flag, int group, bool enabled = true)
		: mLock(&flag), mEnabled(enabled)
	{
		if (mEnabled)
		{
			mLock->Lock(group);
		}
	}

	~RivialLockGuardCond()
	{
		if (mEnabled)
		{
			mLock->Unlock();
		}
	}

private:
	RivalGroupType* mLock;
	BIT_FLAG(mEnabled);
};

#define LOCK_GUARD_RIVAL(lock, group, ...)                                           \
	RivialLockGuardCond<FRWRivalLock> __lock_rival_cond(lock, group, ##__VA_ARGS__); \
	HLVM_ATOMIC_THREAD_FENCE()

#if HLVM_ATOMIC_LOCK_ENABLE_PADDING
	#pragma clang diagnostic pop
#endif
#undef HLVM_ATOMIC_LOCK_ENABLE_PADDING
