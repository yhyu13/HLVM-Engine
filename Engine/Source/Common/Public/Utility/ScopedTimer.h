/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Timer.h"
#include "Core/Log.h"

DELCARE_LOG_CATEGORY(LogScopedTimer)

class FScopedTimer
{
public:
	FScopedTimer(const TCHAR* Msg)
		: mMsg(Msg)
	{
		mTimer.Reset();
	}
	~FScopedTimer()
	{
		HLVM_LOG(LogScopedTimer, trace, TXT("{}: took {} seconds"), *mMsg, mTimer.Mark());
	}

private:
	FString mMsg;
	FTimer	mTimer;
};

#if !HLVM_BUILD_RELEASE
	#define HLVM_SCOPED_TIMER(Msg) \
		FScopedTimer __timer       \
		{                          \
			Msg                    \
		}
#else
	#define HLVM_SCOPED_TIMER(...)
#endif
