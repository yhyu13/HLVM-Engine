/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Test.h"

#include "Core/Log.h"

/*
	<test method>
*/
RECORD(logger_test,
	{
		auto LogDevice = std::make_shared<FSpdlogConsoleDevice>();
		FLogRedirector::Get()->AddDevice(LogDevice);

		// 1, Disable
		LogDevice->Disable();
		HLVM_LOG(LogTemp, critical, TXT("This message should not be shown!"));

		// 2, Enable
		LogDevice->Enable();
		HLVM_LOG(LogTemp, info, TXT("This message should be shown!"));
	})

int main()
{
	// Run all registered test functions
	for (auto& test_function : recorded_test_functions)
	{
		test_function();
	}
	return 0;
}