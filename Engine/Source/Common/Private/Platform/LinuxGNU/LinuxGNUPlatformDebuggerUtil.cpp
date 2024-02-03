/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Platform/PlatformDefinition.h"

#ifdef PLATFORM_LINUXGNU
	#include "Platform/GenericPlatformDebuggerUtil.h"
	#include "Ultility/Timer.h"
	#include "Core/Parallel/Lock.h"

	#include <sys/ptrace.h>

class LinuxGNUPlatformDebuggerUtil final : public GenericPlatformDebuggerUtil
{
protected:
	virtual bool IsDebuggerPresentInternal() final override
	{
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

std::unique_ptr<GenericPlatformDebuggerUtil>
	GenericPlatformDebuggerUtil::s_instance{ new LinuxGNUPlatformDebuggerUtil() };

#endif