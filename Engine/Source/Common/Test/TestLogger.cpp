/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Test.h"

#include "Core/Log.h"
#include "Platform/GenericPlatformDebuggerUtil.h"

DELCARE_LOG_CATEGORY(LogTest)
DEFINE_LOG_CATEGORY(LogTest)

/*
	<test method>
*/
static bool test_logger_test()
{
	HLVM_LOG(LogTest, trace, TXT("Hello World!"));

	// 1, Disable
	auto LogDevice = FLogRedirector::Get()->AllDevices().front();
	LogDevice->Disable();
	HLVM_LOG(LogTest, critical, TXT("This message should not be shown!"));

	// 2, Enable
	LogDevice->Enable();
	HLVM_LOG(LogTest, info, TXT("This message should be shown!"));
	HLVM_LOG(LogTest, info, TXT("This message is formatted: {0}"), TXT("Hi"));
	FString msg = FString::Format(TXT("This message is also formatted: {0}"), TXT("Hi2"));
	HLVM_LOG(LogTest, info, *msg);

	return true;
};
RECORD_TEST_FUNC(logger_test);
