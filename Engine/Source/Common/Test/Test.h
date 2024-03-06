/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Log.h"
#include "Core/String.h"
#include "Core/Assert.h"
#include "Utility/Timer.h"

#include <iostream>
#include <vector>
#include <functional>
#include <chrono>

inline std::vector<std::function<void()>> recorded_test_functions{};

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

/** Macro to record a test function
 * Requirement : (1) use static method in a .cpp test file
 *               (2) function name prefix "test_"
 * static bool test_hash_test()
 * {
 *     ...
 * };
 * RECORD_TEST_FUNC(hash_test);
 */
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
		inline AutoRegister auto_register_##test_function = AutoRegister();                                 \
	}

/**
 * Macro to record a test function (easier version)
 * Example :
 * RECORD(hash_test)
 * {
 *    ...
 * }
 */
#define RECORD(test_function)        \
	void test_##test_function();     \
	RECORD_TEST_FUNC(test_function); \
	void test_##test_function()

// Implement smoothed average time measurement
// i.e. run test case multiple times with timer and calculate average by removing max and min
using TestFuncType = std::function<bool(double&)>;
inline double RunTestAndCalculateAvg(const TestFuncType& func, int num_iterations)
{
	std::vector<double> times;
	for (int i = 0; i < num_iterations; ++i)
	{
		double duration;
		HLVM_ENSURE(func(duration), TXT("Test case failed"));
		times.emplace_back(duration);
	}
	// Remove max and min duration from data collected (by moving them to the end of the array)
	{
		auto mm = std::minmax_element(begin(times), end(times));
		std::iter_swap(mm.first, end(times) - 2);
		std::iter_swap(mm.second, end(times) - 1);
	}
	// mCount average on the rest of data
	double avg = 0.0;
	for (int i = 0; i < num_iterations - 2; ++i)
	{
		avg += times[static_cast<unsigned long>(i)];
	}
	return avg / (num_iterations - 2);
}

int main()
{
	{
		InitMallocator();
	}
	{
		auto LogDevice = std::make_shared<FSpdlogConsoleDevice>();
		FLogRedirector::Get()->AddDevice(LogDevice);
	}

	// Run all registered test functions
	for (auto& test_function : recorded_test_functions)
	{
		test_function();
	}
	return 0;
}
