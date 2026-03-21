/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "Test.h"

#include "Platform/GenericPlatformDebuggerUtil.h"
#include "Core/Log.h"
#include "Core/Assert.h"

DECLARE_LOG_CATEGORY(LogTest)

#define SIMULATE_SEGFAULT 0

/*
	<test method>
*/
RECORD_BOOL(excpetion_test)
{
	// Try to simulate exception
	try
	{
		if (HLVM_IS_DEBUGGER_PRESENT())
		{
			HLVM_LOG(LogTest, warn, TXT("If there is a attached debugger, you will get debug pause after assertion failed. Press continue to finish this test."));
		}
		// Ensure runs in all build config
		HLVM_ENSURE_F(1 != 1, TXT("1 != 1"));
	}
	catch (const std::runtime_error& e)
	{
		HLVM_LOG(LogTest, warn, TO_TCHAR_CSTR(e.what()));
	}

	// Assert would be ignored in release build
	try
	{
		HLVM_ASSERT_F(false, TXT("false"));
	}
	catch (const std::runtime_error& e)
	{
		HLVM_LOG(LogTest, warn, TO_TCHAR_CSTR(e.what()));
	}

#if SIMULATE_SEGFAULT
	// SImulate segfault
	HLVM_SEGFAULT_INLINE();
#endif
	return true;
};
