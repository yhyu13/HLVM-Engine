/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Platform/PlatformDefinition.h"

#ifdef PLATFORM_WINDOWS
	#include "Platform/GenericPlatformDebuggerUtil.h"
	#include "Ultility/Timer.h"
	#include "Core/Parallel/Lock.h"

	// https://medium.com/@X3non_C0der/anti-debugging-techniques-eda1868e0503
	#include <debugapi.h>

class WindowsPlatformDebuggerUtil final : public GenericPlatformDebuggerUtil
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
			BOOL bDebuggerPresent;
			if (IsDebuggerPresent()
				|| (TRUE == CheckRemoteDebuggerPresent(GetCurrentProcess(), &bDebuggerPresent) && TRUE == bDebuggerPresent))
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
	GenericPlatformDebuggerUtil::s_instance{ new WindowsPlatformDebuggerUtil() };

#endif