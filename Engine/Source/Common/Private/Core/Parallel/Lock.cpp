/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Parallel/Lock.h"
#include "Core/Assert.h"
#include "Utility/Timer.h"

#include <emmintrin.h>

#if HLVM_DEADLOCK_TIMER
	#define INIT_DEADLOCK_TIMER() FTimer _timer
	#define ASSERT_DEADLOCK_TIMER() HLVM_ENSURE(_timer.Mark() < 10., TXT("Dead lock after 10s"))
#else
	#define INIT_DEADLOCK_TIMER() void(0)
	#define ASSERT_DEADLOCK_TIMER() void(0)
#endif

#define THREAD_PAUSE() _mm_pause()				 // pause for ~75 clocks on intel 11th-i7 11700
#define THREAD_YIELD() std::this_thread::yield() // pause for ~400 clocks on intel 11th-i7 11700
#define SPIN_COUNT 8							 // ~=1.5 * 400 / 75

#if 1
	/**
	 * Balance spin lock with pause instruction and thread yield
	 * to lower the meaningless power consumptions of CPU during busy waiting
	 */
	#define LOCK_BODY(lock)                                                          \
		INIT_DEADLOCK_TIMER();                                                       \
		while ((lock)->test_and_set(std::memory_order_acq_rel))                      \
		{                                                                            \
			int spin_count = SPIN_COUNT;                                             \
			do                                                                       \
			{                                                                        \
				THREAD_PAUSE();                                                      \
			}                                                                        \
			while ((lock)->test_and_set(std::memory_order_acq_rel) && --spin_count); \
			if (spin_count > 0)                                                      \
			{                                                                        \
				break;                                                               \
			}                                                                        \
			ASSERT_DEADLOCK_TIMER();                                                 \
			THREAD_YIELD();                                                          \
		}                                                                            \
		ATOMIC_THREAD_FENCE()
#else
	#define LOCK_BODY(lock)                                     \
		while ((lock)->test_and_set(std::memory_order_acq_rel)) \
		{                                                       \
		}                                                       \
		ATOMIC_THREAD_FENCE()
#endif

#define UNLOCK_BODY(lock)  \
	ATOMIC_THREAD_FENCE(); \
	(lock)->clear(std::memory_order_release)

FAtomicLockGuard::FAtomicLockGuard(std::atomic_flag& flag) noexcept(!HLVM_DEADLOCK_TIMER)
	: mLock(&flag)
{
	LOCK_BODY(mLock);
}

FAtomicLockGuard::~FAtomicLockGuard() noexcept
{
	UNLOCK_BODY(mLock);
}

void FAtomicFlagStatic::Lock() noexcept(!HLVM_DEADLOCK_TIMER)
{
	LOCK_BODY(&sc_flag);
}

void FAtomicFlagStatic::Unlock() noexcept
{
	UNLOCK_BODY(&sc_flag);
}

void FAtomicFlagNI::Lock() noexcept(!HLVM_DEADLOCK_TIMER)
{
	LOCK_BODY(&ni_flag);
}

void FAtomicFlagNI::Unlock() noexcept
{
	UNLOCK_BODY(&ni_flag);
}

void FAtomicFlagNC::Lock() const noexcept(!HLVM_DEADLOCK_TIMER)
{
	LOCK_BODY(&nc_flag);
}

void FAtomicFlagNC::Unlock() const noexcept
{
	UNLOCK_BODY(&nc_flag);
}

void FAtomicFlag::Lock() const noexcept(!HLVM_DEADLOCK_TIMER)
{
	LOCK_BODY(&mFlag);
}

void FAtomicFlag::Unlock() const noexcept
{
	UNLOCK_BODY(&mFlag);
}

void FRecursiveAtomicFlag::Lock() const noexcept(!HLVM_DEADLOCK_TIMER)
{
	// Test if the same thread already is held
	if (mOwner == GCurrentThreadID)
	{
		mCount.fetch_add(1, std::memory_order_relaxed);
		return;
	}

	// Lock the flag
	LOCK_BODY(&mFlag);

	// Set the owner
	mOwner = GCurrentThreadID;
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
	mOwner = std::thread::id();
	UNLOCK_BODY(&mFlag);
}

void FRWRivalLock::Lock(int group) const noexcept(!HLVM_DEADLOCK_TIMER)
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
		int spin_count = SPIN_COUNT;
		do
		{
			THREAD_PAUSE();
		}
		while (!FGenericPlatformAtomicPointer::AtomicCompareExchange(&mCurrentGroupPtr, &_expected, desiredGroupPtr) && --spin_count);
		if (spin_count)
		{
			break;
		}
		ASSERT_DEADLOCK_TIMER();
		THREAD_YIELD();
	}

	// Add to program counter
	mProgramCounter.fetch_add(1, std::memory_order_relaxed);
}

void FRWRivalLock::Unlock() const noexcept
{
	// If mProgramCounter == 0, it means that the lock is not held by any rival group, so we can reset mCurrentGroupPtr to nullptr
	if (mProgramCounter.fetch_sub(1, std::memory_order_relaxed) == 1)
	{
		mCurrentGroupPtr = nullptr;
	}
}
