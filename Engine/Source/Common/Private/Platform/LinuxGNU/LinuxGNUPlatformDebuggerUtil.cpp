/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Platform/PlatformDefinition.h"

#ifdef PLATFORM_LINUXGNU
	#include "Platform/GenericPlatformDebuggerUtil.h"
	#include "Utility/Timer.h"
	#include "Core/Parallel/Lock.h"

// https://opensource.com/article/18/1/how-debuggers-work
// https://forum.juce.com/t/detecting-if-a-process-is-being-run-under-a-debugger/2098
	#include <sys/ptrace.h>

class FLinuxGNUPlatformDebuggerUtil final : public FGenericPlatformDebuggerUtil
{
protected:
	virtual bool InternalIsDebuggerPresent() final override
	{
		using namespace std::chrono_literals;

		static FTimer	   PeriodicTimer{ 1s, true };
		static int		   underDebugger = 0;
		static bool		   isCheckedAlready = false;
		static FAtomicFlag Lock;
		ATOMIC_LOCK_GUARD(Lock);
		if (!isCheckedAlready || PeriodicTimer.Check(false))
		{
			if (ptrace(PTRACE_TRACEME, 0, 1, 0) < 0
	#if HLVM_BUILD_RELEASE && !HLVM_ALLOW_DEBUGGER_EVEN_IN_RELEASE
				// In release build, we also check if possible to detach from a debugger
				|| ptrace(PTRACE_DETACH, 0, 1, 0) >= 0
	#endif
			)
			{
				underDebugger = 1;
			}
			else
			{
				underDebugger = 0;
			}

			PeriodicTimer.Reset();
			isCheckedAlready = true;
		}
		return underDebugger == 1;
	}
};

FGenericPlatformDebuggerUtil* FGenericPlatformDebuggerUtil::sInstance{ new FLinuxGNUPlatformDebuggerUtil() };

#endif
