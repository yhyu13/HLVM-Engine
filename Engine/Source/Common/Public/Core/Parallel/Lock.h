#pragma once

#include "GlobalDefinition.h"
#include "Platform/PlatformDefinition.h"

#include <atomic>

#define ATOMIC_LOCK_ENABLE_PADDING 1
#define ATOMIC_THREAD_FENCE std::atomic_thread_fence(std::memory_order_acq_rel)

class FAtomicFlag;
class FAtomicLockGuard
{
public:
	NOCOPY(FAtomicLockGuard);
	FAtomicLockGuard() = delete;
	explicit FAtomicLockGuard(std::atomic_flag& flag) noexcept;
	explicit FAtomicLockGuard(FAtomicFlag& Flag) = delete;

	~FAtomicLockGuard() noexcept;

	friend FAtomicFlag;

private:
	std::atomic_flag* m_lock;
};

class FAtomicFlagStatic
{
public:
#define LOCK_GUARD_S()                        \
	FAtomicLockGuard __lock_guard_s(sc_flag); \
	ATOMIC_THREAD_FENCE

	static void LockS() noexcept;
	static void UnLockS() noexcept;

protected:
	// protected non-virtual destructor that prevents delete by base pointer
	~FAtomicFlagStatic() noexcept = default;

protected:
	HLVM_CACHE_ALIGN inline static std::atomic_flag sc_flag; // c++ 20 default initialization to false
};

class FAtomicFlagNI
{
public:
	NOINSTANT(FAtomicFlagNI);
#define LOCK_GUARD_NI()                        \
	FAtomicLockGuard __lock_guard_ni(ni_flag); \
	ATOMIC_THREAD_FENCE

	static void LockNI() noexcept;
	static void UnLockNI() noexcept;

protected:
	HLVM_CACHE_ALIGN inline static std::atomic_flag ni_flag; // c++ 20 default initialization to false
};

class FAtomicFlagNC
{
public:
	NOCOPY(FAtomicFlagNC);

#define LOCK_GUARD_NC()                        \
	FAtomicLockGuard __lock_guard_nc(nc_flag); \
	ATOMIC_THREAD_FENCE

	FAtomicFlagNC() = default;

	void LockNC() const noexcept;
	void UnLockNC() const noexcept;

protected:
	// protected non-virtual destructor that prevents delete by base pointer
	~FAtomicFlagNC() noexcept = default;

protected:
	mutable std::atomic_flag nc_flag;

private:
#if ATOMIC_LOCK_ENABLE_PADDING
	PADDING(HLVM_PLATFORM_CACHE_LINE - sizeof(std::atomic_flag));
#endif
};

class FAtomicFlag
{
public:
#define LOCK_GUARD()                        \
	FAtomicLockGuard __lock_guard_(m_flag); \
	ATOMIC_THREAD_FENCE

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
	// protected non-virtual destructor that prevents delete by base pointer
	~FAtomicFlag() noexcept = default;

protected:
	mutable std::atomic_flag m_flag;

private:
#if ATOMIC_LOCK_ENABLE_PADDING
	PADDING(HLVM_PLATFORM_CACHE_LINE - sizeof(std::atomic_flag));
#endif
};