/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Test.h"

#include "Core/Log.h"
#include "Core/Assert.h"

DELCARE_LOG_CATEGORY(LogTest)
DEFINE_LOG_CATEGORY(LogTest)

/*
	<test method>
*/
RECORD(excpetion_test,
	{
		try
		{
			if (HLVM_IS_DEBUGGER_PRESENT())
			{
				HLVM_LOG(LogTest, warn, TXT("If there is a attached debugger, you will get debug pause after assertion failed. Press continue to finish this test."));
			}
			HLVM_ENSURE(1 != 1, TXT("1 != 1"));
		}
		catch (const std::runtime_error& e)
		{
			HLVM_LOG(LogTest, debug, TO_TCHAR_STR(e.what()));
		}
	})