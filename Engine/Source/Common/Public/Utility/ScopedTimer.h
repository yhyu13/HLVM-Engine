/**
 * Copyright (c) 2024. MIT License. All rights reserved.
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
		HLVM_LOG(LogScopedTimer, trace, TXT("{}: took {} sec"), *mMsg, mTimer.Mark());
	}

private:
	FString mMsg;
	FTimer	mTimer;
};

#if !HLVM_BUILD_RELEASE
	#define HLVM_SCOPED_TIMER_LOG(Msg)                   \
		FScopedTimerLog TOKENPASTE2(__timer_, __LINE__){ \
			Msg                                          \
		};                                               \
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

#define HLVM_SCOPED_TIMER(Duration)               \
	FScopedTimer TOKENPASTE2(__timer_, __LINE__){ \
		Duration                                  \
	};                                            \
	HLVM_ATOMIC_THREAD_FENCE()

template <typename ratio = std::ratio<1>, typename duration_type = double>
class FScopedTimerCum
{
public:
	FScopedTimerCum() = delete;
	explicit FScopedTimerCum(duration_type& Duration)
		: mDuration(&Duration)
	{
		mTimer.Reset();
	}

	~FScopedTimerCum()
	{
		*mDuration += mTimer.Mark<ratio, duration_type>();
	}

private:
	duration_type* mDuration;
	FTimer		   mTimer;
};

#define HLVM_SCOPED_TIMER_CUM(Duration)              \
	FScopedTimerCum TOKENPASTE2(__timer_, __LINE__){ \
		Duration                                     \
	};                                               \
	HLVM_ATOMIC_THREAD_FENCE()

template <typename ratio = std::ratio<1>, typename duration_type = double>
class FScopedTimerCumAtomic
{
public:
	FScopedTimerCumAtomic() = delete;
	explicit FScopedTimerCumAtomic(std::atomic<duration_type>& Duration)
		: mDuration(&Duration)
	{
		mTimer.Reset();
	}

	~FScopedTimerCumAtomic()
	{
		mDuration->fetch_add(mTimer.Mark<ratio, duration_type>(), std::memory_order_relaxed);
	}

private:
	std::atomic<duration_type>* mDuration;
	FTimer						mTimer;
};

#define HLVM_SCOPED_TIMER_CUM_ATOMIC(Duration, ratio)                                             \
	FScopedTimerCumAtomic<ratio, decltype(Duration)::value_type> TOKENPASTE2(__timer_, __LINE__){ \
		Duration                                                                                  \
	};                                                                                            \
	HLVM_ATOMIC_THREAD_FENCE()
