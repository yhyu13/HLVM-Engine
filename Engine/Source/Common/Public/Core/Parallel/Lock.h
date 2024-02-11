/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "ParallelDefinition.h"

#ifndef __ATOMIC_LOCK_ENABLE_PADDING
	#define __ATOMIC_LOCK_ENABLE_PADDING 1
#endif

#if !HLVM_BUILD_RELEASE
	#define __DEADLOCK_TIMER 1 // Debug break on potential dead lock
#else
	#define __DEADLOCK_TIMER 0
#endif // !HLVM_BUILD_RELEASE

class FAtomicFlag;
class FAtomicLockGuard
{
public:
	NOCOPYMOVE(FAtomicLockGuard);
	FAtomicLockGuard() = delete;
	explicit FAtomicLockGuard(std::atomic_flag& flag) noexcept(!__DEADLOCK_TIMER);
	explicit FAtomicLockGuard(FAtomicFlag& Flag) noexcept(!__DEADLOCK_TIMER);

	~FAtomicLockGuard() noexcept;

private:
	std::atomic_flag* m_lock;
};

#define ATOMIC_LOCK_GUARD(x)            \
	FAtomicLockGuard __lock_guard((x)); \
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

	static void LockS() noexcept(!__DEADLOCK_TIMER);
	static void UnLockS() noexcept;

protected:
	HLVM_CACHE_ALIGN inline static std::atomic_flag sc_flag = ATOMIC_FLAG_INIT;
};

/**
 * @class FAtomicFlagNI
 * @brief 一个非实例原子标志类
 */
class FAtomicFlagNI
{
public:
	NOINSTANT(FAtomicFlagNI);
#define LOCK_GUARD_NI()                        \
	FAtomicLockGuard __lock_guard_ni(ni_flag); \
	ATOMIC_THREAD_FENCE()

	static void LockNI() noexcept(!__DEADLOCK_TIMER);
	static void UnLockNI() noexcept;

protected:
	HLVM_CACHE_ALIGN inline static std::atomic_flag ni_flag = ATOMIC_FLAG_INIT;
};

/**
 * @class FAtomicFlagNC
 * @brief 一个具有非复制的原子标志类
 */
class FAtomicFlagNC
{
public:
	NOCOPYMOVE(FAtomicFlagNC);

#define LOCK_GUARD_NC()                        \
	FAtomicLockGuard __lock_guard_nc(nc_flag); \
	ATOMIC_THREAD_FENCE()

	FAtomicFlagNC() = default;

	void LockNC() const noexcept(!__DEADLOCK_TIMER);
	void UnLockNC() const noexcept;

protected:
	mutable std::atomic_flag nc_flag = ATOMIC_FLAG_INIT;

private:
#if __ATOMIC_LOCK_ENABLE_PADDING
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
#define LOCK_GUARD()                        \
	FAtomicLockGuard __lock_guard_(m_flag); \
	ATOMIC_THREAD_FENCE()

	FAtomicFlag() noexcept = default;

	FAtomicFlag(const FAtomicFlag& other) noexcept
	{
		// Not copy the atomic flag
	}

	FAtomicFlag(FAtomicFlag&& other) noexcept
	{
		// Not copy the atomic flag
	}

	FAtomicFlag& operator=(const FAtomicFlag& rhs) noexcept
	{
		return *this;
	}
	FAtomicFlag& operator=(FAtomicFlag&& rhs) noexcept
	{
		return *this;
	}

	void Lock() const noexcept(!__DEADLOCK_TIMER);
	void UnLock() const noexcept;

protected:
	friend class FAtomicLockGuard;
	mutable std::atomic_flag m_flag = ATOMIC_FLAG_INIT;

private:
#if __ATOMIC_LOCK_ENABLE_PADDING
	PADDING(HLVM_PLATFORM_CACHE_LINE - sizeof(std::atomic_flag));
#endif
};
#undef __ATOMIC_LOCK_ENABLE_PADDING