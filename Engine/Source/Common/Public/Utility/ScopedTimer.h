/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "Timer.h"
#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogScopedTimer)

class FScopedTimerLog
{
public:
	FScopedTimerLog() = delete;
	explicit FScopedTimerLog(const TCHAR* Msg)
		: mMsg(Msg)
	{
		mTimer.Reset();
	}

	~FScopedTimerLog()
	{
		HLVM_LOG(LogScopedTimer, info, TXT("{}: took {} sec"), *mMsg, mTimer.Mark());
	}

private:
	FString mMsg;
	FTimer	mTimer;
};

#if !HLVM_SHIPPING_CODE
	#define HLVM_SCOPED_TIMER_LOG(Msg)             \
		FScopedTimerLog TOKENPASTE2LINE(__timer_){ \
			Msg                                    \
		};                                         \
		HLVM_ATOMIC_THREAD_FENCE()
#else
	#define HLVM_SCOPED_TIMER_LOG(Msg) ((void)0)
#endif

template <typename ratio = std::ratio<1>, typename duration_type = double>
class FScopedTimer
{
public:
	FScopedTimer() = delete;
	explicit FScopedTimer(duration_type& Duration)
		: mDuration(&Duration)
	{
		mTimer.Reset();
	}

	~FScopedTimer()
	{
		*mDuration = mTimer.Mark<ratio, duration_type>();
	}

private:
	duration_type* mDuration;
	FTimer		   mTimer;
};

#if !HLVM_SHIPPING_CODE
	#define HLVM_SCOPED_TIMER(Duration)         \
		FScopedTimer TOKENPASTE2LINE(__timer_){ \
			Duration                            \
		};                                      \
		HLVM_ATOMIC_THREAD_FENCE()
#else
	#define HLVM_SCOPED_TIMER(Msg) ((void)0)
#endif

template <typename ratio = std::ratio<1>, typename duration_type = double>
class FScopedTimerCumu
{
public:
	FScopedTimerCumu() = delete;
	explicit FScopedTimerCumu(duration_type& Duration)
		: mDuration(&Duration)
	{
		mTimer.Reset();
	}

	~FScopedTimerCumu()
	{
		*mDuration += mTimer.Mark<ratio, duration_type>();
	}

private:
	duration_type* mDuration;
	FTimer		   mTimer;
};

#if !HLVM_SHIPPING_CODE
	#define HLVM_SCOPED_TIMER_CUMULATIVE(Duration)  \
		FScopedTimerCumu TOKENPASTE2LINE(__timer_){ \
			Duration                                \
		};                                          \
		HLVM_ATOMIC_THREAD_FENCE()
#else
	#define HLVM_SCOPED_TIMER_CUMULATIVE(Msg) ((void)0)
#endif

template <typename ratio = std::ratio<1>, typename duration_type = double>
class FScopedTimerCumuAtomic
{
public:
	FScopedTimerCumuAtomic() = delete;
	explicit FScopedTimerCumuAtomic(std::atomic<duration_type>& Duration)
		: mDuration(&Duration)
	{
		mTimer.Reset();
	}

	~FScopedTimerCumuAtomic()
	{
		mDuration->fetch_add(mTimer.Mark<ratio, duration_type>(), std::memory_order_relaxed);
	}

private:
	std::atomic<duration_type>* mDuration;
	FTimer						mTimer;
};

#if !HLVM_SHIPPING_CODE
	#define HLVM_SCOPED_TIMER_CUMULATIVE_ATOMIC(Duration, ratio)                                 \
		FScopedTimerCumuAtomic<ratio, decltype(Duration)::value_type> TOKENPASTE2LINE(__timer_){ \
			Duration                                                                             \
		};                                                                                       \
		HLVM_ATOMIC_THREAD_FENCE()
#else
	#define HLVM_SCOPED_TIMER_CUMULATIVE_ATOMIC(Msg) ((void)0)
#endif
