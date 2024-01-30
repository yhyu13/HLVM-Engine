/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Test.h"

#include "Core/Log.h"

DELCARE_LOG_CATEGORY(LogTest)
DEFINE_LOG_CATEGORY(LogTest)

/*
	<test method>
*/
RECORD(logger_test,
	{
		auto LogDevice = std::make_shared<FSpdlogConsoleDevice>();
		FLogRedirector::Get()->AddDevice(LogDevice);
		HLVM_LOG(LogTest, trace, TXT("Hello World!"));

		// 1, Disable
		LogDevice->Disable();
		HLVM_LOG(LogTest, critical, TXT("This message should not be shown!"));

		// 2, Enable
		LogDevice->Enable();
		HLVM_LOG(LogTest, info, TXT("This message should be shown!"));
	})