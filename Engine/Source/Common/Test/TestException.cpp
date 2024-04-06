/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Test.h"

#include "Platform/GenericPlatformDebuggerUtil.h"
#include "Core/Log.h"
#include "Core/Assert.h"

DECLARE_LOG_CATEGORY(LogTest)

/*
	<test method>
*/
RECORD(excpetion_test)
{
	try
	{
		if (HLVM_IS_DEBUGGER_PRESENT())
		{
			HLVM_LOG(LogTest, warn, TXT("If there is a attached debugger, you will get debug pause after assertion failed. Press continue to finish this test."));
		}
		// Ensure runs in all build config
		HLVM_ENSURE(1 != 1, TXT("1 != 1"));
	}
	catch (const std::runtime_error& e)
	{
		HLVM_LOG(LogTest, warn, TO_TCHAR_STR(e.what()));
	}
	try
	{
		// Assert would be ignored in release build
		HLVM_ASSERT(false, TXT("false"));
	}
	catch (const std::runtime_error& e)
	{
		HLVM_LOG(LogTest, warn, TO_TCHAR_STR(e.what()));
	}
};
