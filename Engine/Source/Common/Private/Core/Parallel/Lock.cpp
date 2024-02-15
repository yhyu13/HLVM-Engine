/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Parallel/Lock.h"
#include "Core/Assert.h"
#include "Ultility/Timer.h"

#include <emmintrin.h>

#if HLVM_DEADLOCK_TIMER
	#define INIT_DEADLOCK_TIMER() FTimer _timer
	#define ASSERT_DEADLOCK_TIMER() HLVM_ENSURE(_timer.Mark() < 10., TXT("Dead lock after 10s"))
#else
	#define INIT_DEADLOCK_TIMER()
	#define ASSERT_DEADLOCK_TIMER()
#endif

#define THREAD_PAUSE() _mm_pause()				 // pause for ~75 clocks on intel 11th-i7 11700
#define THREAD_YIELD() std::this_thread::yield() // pause for ~400 clocks on intel 11th-i7 11700
#define SPIN_COUNT 8							 // ~=1.5 * 400 / 75

#if 1
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
			ATOMIC_THREAD_FENCE();                                                   \
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
	: m_lock(&flag)
{
	LOCK_BODY(m_lock);
}

FAtomicLockGuard::FAtomicLockGuard(FAtomicFlag& Flag) noexcept(!HLVM_DEADLOCK_TIMER)
	: m_lock(&Flag.m_flag)
{
	LOCK_BODY(m_lock);
}

FAtomicLockGuard::FAtomicLockGuard(std::optional<FAtomicFlag>& Flag) noexcept(!HLVM_DEADLOCK_TIMER)
	: m_lock((Flag) ? &Flag->m_flag : nullptr)
{
	if (!m_lock) [[unlikely]]
		return;
	LOCK_BODY(m_lock);
}

FAtomicLockGuard::~FAtomicLockGuard() noexcept
{
	if (!m_lock) [[unlikely]]
		return;
	UNLOCK_BODY(m_lock);
}

void FAtomicFlagStatic::LockS() noexcept(!HLVM_DEADLOCK_TIMER)
{
	LOCK_BODY(&sc_flag);
}

void FAtomicFlagStatic::UnLockS() noexcept
{
	UNLOCK_BODY(&sc_flag);
}

void FAtomicFlagNI::LockNI() noexcept(!HLVM_DEADLOCK_TIMER)
{
	LOCK_BODY(&ni_flag);
}

void FAtomicFlagNI::UnLockNI() noexcept
{
	UNLOCK_BODY(&ni_flag);
}

void FAtomicFlagNC::LockNC() const noexcept(!HLVM_DEADLOCK_TIMER)
{
	LOCK_BODY(&nc_flag);
}

void FAtomicFlagNC::UnLockNC() const noexcept
{
	UNLOCK_BODY(&nc_flag);
}

void FAtomicFlag::Lock() const noexcept(!HLVM_DEADLOCK_TIMER)
{
	LOCK_BODY(&m_flag);
}

void FAtomicFlag::UnLock() const noexcept
{
	UNLOCK_BODY(&m_flag);
}
