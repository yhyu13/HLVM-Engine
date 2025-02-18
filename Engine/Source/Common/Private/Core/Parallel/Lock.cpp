/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Core/Parallel/Lock.h"
#include "Core/Assert.h"
#include "Utility/Timer.h"

#include <emmintrin.h>

#if HLVM_DEADLOCK_TIMER
	#define INIT_DEADLOCK_TIMER() FTimer _timer
	#define ASSERT_DEADLOCK_TIMER() HLVM_ENSURE_F(_timer.Mark() < 10., TXT("Dead lock after 10s"))
#else
	#define INIT_DEADLOCK_TIMER() void(0)
	#define ASSERT_DEADLOCK_TIMER() void(0)
#endif

#define THREAD_PAUSE() _mm_pause()				 // pause for ~75 clocks on intel 11th-i7 11700
#define THREAD_YIELD() std::this_thread::yield() // pause for ~400 clocks on intel 11th-i7 11700
#define PAUSE_SPIN_COUNT 32						 //

// Inspired from boost spinlock_ttas.hpp, use thread sleep and contention spin
// to further reduce cpu stall and improve lock efficiency
// based on informations from:
// https://software.intel.com/en-us/articles/benefitting-power-and-performance-sleep-loops
// https://software.intel.com/en-us/articles/long-duration-spin-wait-loops-on-hyper-threading-technology-enabled-intel-processors
HLVM_STATIC_VAR constexpr std::chrono::microseconds us0{ 0 };
#define THREAD_SLEEP0() std::this_thread::sleep_for(us0) // pause for 1000 clocks claimed by boost?
#define SLEEP_SPIN_COUNT 32								 //
#define CONTENTION_SPIN_COUNT 16						 //

#if 1
	/**
	 * Balance spin lock with pause instruction and thread yield
	 * to lower the meaningless power consumptions of CPU during busy waiting
	 */
	#define LOCK_BODY(lock)                                                      \
		INIT_DEADLOCK_TIMER();                                                   \
		for (;;)                                                                 \
		{                                                                        \
			while ((lock)->test(std::memory_order_relaxed))                      \
			{                                                                    \
				uint_fast8_t spin_count = PAUSE_SPIN_COUNT;                      \
				do                                                               \
				{                                                                \
					THREAD_PAUSE();                                              \
				}                                                                \
				while ((lock)->test(std::memory_order_relaxed) && --spin_count); \
				if (spin_count > 0)                                              \
				{                                                                \
					break;                                                       \
				}                                                                \
				spin_count = SLEEP_SPIN_COUNT;                                   \
				do                                                               \
				{                                                                \
					THREAD_SLEEP0();                                             \
				}                                                                \
				while ((lock)->test(std::memory_order_relaxed) && --spin_count); \
				if (spin_count > 0)                                              \
				{                                                                \
					break;                                                       \
				}                                                                \
				ASSERT_DEADLOCK_TIMER();                                         \
				THREAD_YIELD();                                                  \
			}                                                                    \
			if ((lock)->test_and_set(std::memory_order_acq_rel))                 \
			{                                                                    \
				uint_fast8_t spin_count = CONTENTION_SPIN_COUNT;                 \
				do                                                               \
				{                                                                \
					THREAD_PAUSE();                                              \
				}                                                                \
				while (--spin_count);                                            \
			}                                                                    \
			else                                                                 \
			{                                                                    \
				break;                                                           \
			}                                                                    \
		}                                                                        \
		HLVM_ATOMIC_THREAD_FENCE()
#else
	#define LOCK_BODY(lock)                                     \
		while ((lock)->test_and_set(std::memory_order_acq_rel)) \
		{                                                       \
		}                                                       \
		HLVM_ATOMIC_THREAD_FENCE()
#endif

#define UNLOCK_BODY(lock)       \
	HLVM_ATOMIC_THREAD_FENCE(); \
	(lock)->clear(std::memory_order_release)

namespace hlvm_private
{
	void LockAtomic(std::atomic_flag* flag) HLVM_LOCK_METHOD_NOEXCEPT // extern
	{
		LOCK_BODY(flag);
	}

	void UnlockAtomic(std::atomic_flag* flag) noexcept // extern
	{
		UNLOCK_BODY(flag);
	}
} // namespace hlvm_private

FAtomicLockGuard::FAtomicLockGuard(std::atomic_flag& flag) HLVM_LOCK_METHOD_NOEXCEPT : mLock(&flag)
{
	hlvm_private::LockAtomic(mLock);
}

FAtomicLockGuard::~FAtomicLockGuard() noexcept
{
	hlvm_private::UnlockAtomic(mLock);
}

void FAtomicFlagNC::Lock() const HLVM_LOCK_METHOD_NOEXCEPT
{
	hlvm_private::LockAtomic(&nc_flag);
}

void FAtomicFlagNC::Unlock() const noexcept
{
	hlvm_private::UnlockAtomic(&nc_flag);
}

void FAtomicFlag::Lock() const HLVM_LOCK_METHOD_NOEXCEPT
{
	hlvm_private::LockAtomic(&mFlag);
}

void FAtomicFlag::Unlock() const noexcept
{
	hlvm_private::UnlockAtomic(&mFlag);
}

void FRecursiveAtomicFlag::Lock() const HLVM_LOCK_METHOD_NOEXCEPT
{
	// Test if the same thread already is held
	if (mOwnerTid == GCurrentTID64)
	{
		mCount.fetch_add(1, std::memory_order_relaxed);
		return;
	}

	// Lock the flag
	hlvm_private::LockAtomic(&mFlag);

	// Set the owner
	mOwnerTid = GCurrentTID64;
	mCount = 1;
}

void FRecursiveAtomicFlag::Unlock() const noexcept
{
	// Decrement the counter, early return when not 0
	if (mCount.fetch_sub(1, std::memory_order_relaxed) > 1)
	{
		return;
	}

	// Last held unlock, release owner
	mOwnerTid = 0;
	hlvm_private::UnlockAtomic(&mFlag);
}

void FRWRivalLock::LockRV(int group) const HLVM_LOCK_METHOD_NOEXCEPT
{
	Group* desiredGroupPtr = C_C(Group*, &mGroups[group]);

	// Test if the same group already is held
	if (mCurrentGroupPtr == desiredGroupPtr)
	{
		// If already held by the same rival group, try to add to program counter
		if (mProgramCounter.fetch_add(1, std::memory_order_relaxed) > 0)
		{
			return;
		}
		// But if UnLock happens after 'if (mCurrentGroupPtr == groupPtr)' and before above fetch_add statement,
		// There is a chance that fetch_add returns 0, which allow other rival group to compete for lock, so we need to compete for the lock as
		// And Before competing for the lock, we need to reset mProgramCounter by subtracting 1
		mProgramCounter.fetch_sub(1, std::memory_order_relaxed);
	}

	// Try to compete for the lock
	INIT_DEADLOCK_TIMER();
	Group* _expected = nullptr;
	while (!FGenericPlatformAtomicPointer::AtomicCompareExchange(&mCurrentGroupPtr, &_expected, desiredGroupPtr))
	{
		uint_fast8_t spin_count = PAUSE_SPIN_COUNT;
		do
		{
			THREAD_PAUSE();
		}
		while (!FGenericPlatformAtomicPointer::AtomicCompareExchange(&mCurrentGroupPtr, &_expected, desiredGroupPtr) && --spin_count);
		if (spin_count)
		{
			break;
		}
		spin_count = SLEEP_SPIN_COUNT;
		do
		{
			THREAD_SLEEP0();
		}
		while (!FGenericPlatformAtomicPointer::AtomicCompareExchange(&mCurrentGroupPtr, &_expected, desiredGroupPtr) && --spin_count);
		if (spin_count > 0)
		{
			break;
		}
		ASSERT_DEADLOCK_TIMER();
		THREAD_YIELD();
	}

	// Add to program counter
	mProgramCounter.fetch_add(1, std::memory_order_relaxed);
}

void FRWRivalLock::UnlockRV() const noexcept
{
	// If mProgramCounter == 0, it means that the lock is not held by any rival group, so we can reset mCurrentGroupPtr to nullptr
	if (mProgramCounter.fetch_sub(1, std::memory_order_relaxed) == 1)
	{
		mCurrentGroupPtr = nullptr;
	}
}
