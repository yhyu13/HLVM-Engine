/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "ParallelDefinition.h"

#ifndef ATOMIC_LOCK_ENABLE_PADDING
	#define ATOMIC_LOCK_ENABLE_PADDING 1
#endif

class FAtomicFlag;
class FAtomicLockGuard
{
public:
	NOCOPY(FAtomicLockGuard);
	FAtomicLockGuard() = delete;
	explicit FAtomicLockGuard(std::atomic_flag& flag) noexcept;
	explicit FAtomicLockGuard(FAtomicFlag& Flag) noexcept;

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

	static void LockS() noexcept;
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

	static void LockNI() noexcept;
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
	NOCOPY(FAtomicFlagNC);

#define LOCK_GUARD_NC()                        \
	FAtomicLockGuard __lock_guard_nc(nc_flag); \
	ATOMIC_THREAD_FENCE()

	FAtomicFlagNC() = default;

	void LockNC() const noexcept;
	void UnLockNC() const noexcept;

protected:
	mutable std::atomic_flag nc_flag = ATOMIC_FLAG_INIT;

private:
#if ATOMIC_LOCK_ENABLE_PADDING
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

	void Lock() const noexcept;
	void UnLock() const noexcept;

protected:
	friend class FAtomicLockGuard;
	mutable std::atomic_flag m_flag = ATOMIC_FLAG_INIT;

private:
#if ATOMIC_LOCK_ENABLE_PADDING
	PADDING(HLVM_PLATFORM_CACHE_LINE - sizeof(std::atomic_flag));
#endif
};
#undef ATOMIC_LOCK_ENABLE_PADDING