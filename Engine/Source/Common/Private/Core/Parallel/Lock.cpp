#include "Core/Parallel/Lock.h"
#include "Core/Assert.h"
#include "Core/Time/Timer.h"

#include <emmintrin.h>

#if !HLVM_BUILD_RELEASE
	#define DEADLOCK_TIMER 1 // Debug break unfriendly, disabled unless you need to debug dead lock
#else
	#define DEADLOCK_TIMER 0
#endif // !HLVM_BUILD_RELEASE

#if DEADLOCK_TIMER
	#define SET_DEADLOCK_TIMER() FTimer _timer
	#define ASSERT_DEADLOCK_TIMER() ASSERT(_timer.Mark() < 10.f, TXT("Dead lock after 10s"))
#else
	#define SET_DEADLOCK_TIMER()
	#define ASSERT_DEADLOCK_TIMER()
#endif

#define THREAD_PAUSE() _mm_pause()				 // pause for ~75 clocks on intel 11th-i7 11700
#define THREAD_YIELD() std::this_thread::yield() // pause for ~400 clocks on intel 11th-i7 11700
#define SPIN_COUNT 8

FAtomicLockGuard::FAtomicLockGuard(std::atomic_flag& flag) noexcept
	: m_lock(&flag)
{
	SET_DEADLOCK_TIMER();
	while (m_lock->test_and_set(std::memory_order_acq_rel))
	{
		int spin_count = SPIN_COUNT;
		do
		{
			THREAD_PAUSE();
		}
		while (m_lock->test_and_set(std::memory_order_acq_rel) && --spin_count);
		if (spin_count)
		{
			break;
		}
		ASSERT_DEADLOCK_TIMER();
		THREAD_YIELD();
	}
}

FAtomicLockGuard::~FAtomicLockGuard() noexcept
{
	m_lock->clear(std::memory_order_release);
}

void FAtomicFlagStatic::LockS() noexcept
{
	SET_DEADLOCK_TIMER();
	while (sc_flag.test_and_set(std::memory_order_acq_rel))
	{
		int spin_count = SPIN_COUNT;
		do
		{
			THREAD_PAUSE();
		}
		while (sc_flag.test_and_set(std::memory_order_acq_rel) && --spin_count);
		if (spin_count)
		{
			break;
		}
		ASSERT_DEADLOCK_TIMER();
		THREAD_YIELD();
	}
}

void FAtomicFlagStatic::UnLockS() noexcept
{
	sc_flag.clear(std::memory_order_release);
}

void FAtomicFlagNI::LockNI() noexcept
{
	SET_DEADLOCK_TIMER();
	while (ni_flag.test_and_set(std::memory_order_acq_rel))
	{
		int spin_count = SPIN_COUNT;
		do
		{
			THREAD_PAUSE();
		}
		while (ni_flag.test_and_set(std::memory_order_acq_rel) && --spin_count);
		if (spin_count)
		{
			break;
		}
		ASSERT_DEADLOCK_TIMER();
		THREAD_YIELD();
	}
}

void FAtomicFlagNI::UnLockNI() noexcept
{
	ni_flag.clear(std::memory_order_release);
}

void FAtomicFlagNC::LockNC() const noexcept
{
	SET_DEADLOCK_TIMER();
	while (nc_flag.test_and_set(std::memory_order_acq_rel))
	{
		int spin_count = SPIN_COUNT;
		do
		{
			THREAD_PAUSE();
		}
		while (nc_flag.test_and_set(std::memory_order_acq_rel) && --spin_count);
		if (spin_count)
		{
			break;
		}
		ASSERT_DEADLOCK_TIMER();
		THREAD_YIELD();
	}
}

void FAtomicFlagNC::UnLockNC() const noexcept
{
	nc_flag.clear(std::memory_order_release);
}

void FAtomicFlag::Lock() const noexcept
{
	SET_DEADLOCK_TIMER();
	while (m_flag.test_and_set(std::memory_order_acq_rel))
	{
		int spin_count = SPIN_COUNT;
		do
		{
			THREAD_PAUSE();
		}
		while (m_flag.test_and_set(std::memory_order_acq_rel) && --spin_count);
		if (spin_count)
		{
			break;
		}
		ASSERT_DEADLOCK_TIMER();
		THREAD_YIELD();
	}
}

void FAtomicFlag::UnLock() const noexcept
{
	m_flag.clear(std::memory_order_release);
}
