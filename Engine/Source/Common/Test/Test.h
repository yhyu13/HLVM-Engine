/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Log.h"
#include "Core/String.h"
#include "Core/Assert.h"
#include "Ultility/Timer.h"

#include <iostream>
#include <vector>
#include <functional>
#include <chrono>

static std::vector<std::function<void()>> recorded_test_functions;

// Helper function to create a lambda that runs the test and prints the info
template <typename Func>
std::function<void()> make_test_wrapper(const FString& name, Func test_function)
{
	return [name, test_function]() {
		HLVM_LOG(LogTemp, info, TXT("Running {}"), *name);
		FTimer Timer{ true };
		// check if test_function has return type bool
		if constexpr (std::is_same_v<decltype(test_function()), bool>)
		{
			HLVM_ENSURE(test_function(), TXT("Test failed {}"), *name); // Run the actual test function
		}
		else
		{
			test_function();
		}
		HLVM_LOG(LogTemp, info, TXT("Completed {} in {} seconds"), *name, Timer.MarkSec());
	};
}

// Macro to record a test function
#define RECORD_TEST_FUNC(test_function)                                                                     \
	namespace AutoRegister_##test_function                                                                  \
	{                                                                                                       \
		struct AutoRegister                                                                                 \
		{                                                                                                   \
			AutoRegister()                                                                                  \
			{                                                                                               \
				recorded_test_functions.push_back(make_test_wrapper(#test_function, test_##test_function)); \
			}                                                                                               \
		};                                                                                                  \
		AutoRegister auto_register_##test_function = AutoRegister();                                        \
	}

#define RECORD(test_function, ...) \
	void test_##test_function()    \
	{                              \
		__VA_ARGS__;               \
	};                             \
	RECORD_TEST_FUNC(test_function);

int main()
{
	auto LogDevice = std::make_shared<FSpdlogConsoleDevice>();
	FLogRedirector::Get()->AddDevice(LogDevice);

	// Run all registered test functions
	for (auto& test_function : recorded_test_functions)
	{
		test_function();
	}
	return 0;
}