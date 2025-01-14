/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Log.h"
#include "Core/String.h"
#include "Core/Assert.h"
#include "Core/Mallocator/PMR.h"
#include "Platform/GenericPlatformCrashDump.h"
#include "Platform/GenericPlatformFile.h"

#include "Utility/ScopedTimer.h"
#include "Utility/Profiler/ProfilerCPU.h"

#include <iostream>
#include <vector>
#include <functional>
#include <chrono>

namespace hlvm_private
{
	HLVM_INLINE_VAR std::vector<std::function<void()>> recorded_test_functions{};
}

/**
 * TestContext
 */
struct TestContext
{
	bool	 bEnabled = true;
	uint32_t randomSeed = 0;
	uint32_t repeat = 1;
};

// Helper function to create a lambda that runs the test and prints the info
template <typename Func>
std::function<void()> _make_test_wrapper(const FString& name, Func test_function, const TestContext& ctx)
{
	return [name, test_function, ctx]() {
		if (!ctx.bEnabled)
		{
			HLVM_LOG(LogTemp, info, TXT("Skipping {} bc it is disabled"), *name);
			return;
		}

		for (uint32_t _i = 0u; _i < ctx.repeat; ++_i)
		{
			FTimer Timer{ true };
			// Seed the random number generator
			std::srand(ctx.randomSeed);
			HLVM_LOG(LogTemp, info, TXT("Running {} (#{})"), *name, _i + 1);
			// Run the actual test function
			// check if test_function has return type bool
			if constexpr (std::is_same_v<decltype(test_function()), bool>)
			{
				HLVM_ENSURE(test_function(), TXT("Test failed {}, return false"), *name);
			}
			if constexpr (std::is_same_v<decltype(test_function()), int>)
			{
				int ret = test_function();
				HLVM_ENSURE(ret == 0, TXT("Test failed {}, return {}"), *name, ret);
			}
			else
			{
				test_function();
			}
			HLVM_LOG(LogTemp, info, TXT("Completed {} (#{}) in {} seconds"), *name, _i + 1, Timer.MarkSec());
		}
	};
}

#define RECORD_TEST_FUNC_BODY(test_function)                                                                                                 \
	struct AutoRegister                                                                                                                      \
	{                                                                                                                                        \
		AutoRegister()                                                                                                                       \
		{                                                                                                                                    \
			hlvm_private::recorded_test_functions.push_back(_make_test_wrapper(#test_function, test_##test_function, _AutoRegisterContext)); \
		}                                                                                                                                    \
	};                                                                                                                                       \
	static AutoRegister _AutoRegister

#define RECORD_TEST_FUNC1(test_function, ...)                 \
	HLVM_STATIC_VAR TestContext _AutoRegisterContext{ true }; \
	RECORD_TEST_FUNC_BODY(test_function);

#define RECORD_TEST_FUNC2(test_function, ...)                        \
	HLVM_STATIC_VAR TestContext _AutoRegisterContext{ __VA_ARGS__ }; \
	RECORD_TEST_FUNC_BODY(test_function);

#define RECORD_TEST_FUNC(test_function, ...)                                          \
	namespace record_##test_function                                                  \
	{                                                                                 \
		constexpr auto ArgCount = HLVM_GET_ARGS_COUNT(#test_function, ##__VA_ARGS__); \
		struct RecordTestFunc                                                         \
		{                                                                             \
			RecordTestFunc()                                                          \
			{                                                                         \
				if constexpr (ArgCount == 1)                                          \
				{                                                                     \
					RECORD_TEST_FUNC1(test_function, ##__VA_ARGS__);                  \
				}                                                                     \
				else                                                                  \
				{                                                                     \
					RECORD_TEST_FUNC2(test_function, ##__VA_ARGS__);                  \
				}                                                                     \
			}                                                                         \
		};                                                                            \
		static RecordTestFunc _RecordTestFunc;                                        \
	}

/**
 * Macro to record a test function
 * Example : name, enable, seed, repeat
 * RECORD(test_func, true, 0, 1)
 * {
 *    ...
 *    return;
 * }
 */
#define RECORD(test_function, ...)                  \
	void test_##test_function();                    \
	RECORD_TEST_FUNC(test_function, ##__VA_ARGS__); \
	void test_##test_function()

/**
 * Macro to record a test function
 * Example : name, enable, seed, repeat
 * RECORD_BOOL(test_func, true, 0, 1)
 * {
 *    ...
 *    return true;  // succeed
 * }
 */
#define RECORD_BOOL(test_function, ...)             \
	bool test_##test_function();                    \
	RECORD_TEST_FUNC(test_function, ##__VA_ARGS__); \
	bool test_##test_function()

/**
 * Macro to record a test function
 * Example : name, enable, seed, repeat
 * RECORD_INT(test_func, true, 0, 1)
 * {
 *    ...
 *    return 0; // succeed
 * }
 */
#define RECORD_INT(test_function, ...)              \
	int test_##test_function();                     \
	RECORD_TEST_FUNC(test_function, ##__VA_ARGS__); \
	int test_##test_function()

#define SECTION(section_name, enabled, repeat, ...)             \
	do                                                          \
	{                                                           \
		if (enabled)                                            \
		{                                                       \
			std::function<void()> func_##section_name = [&]() { \
				do                                              \
				{                                               \
					__VA_ARGS__                                 \
				}                                               \
				while (0);                                      \
			};                                                  \
			for (uint32_t _i = 0u; _i < repeat; ++_i)           \
			{                                                   \
				func_##section_name();                          \
			}                                                   \
		}                                                       \
	}                                                           \
	while (0)

// Implement smoothed average time measurement
// i.e. run test case multiple times with timer and calculate average by removing max and min
using TestFuncType = std::function<bool(double&)>;
inline double RunTestAndCalculateAvg(const TestFuncType& func, uint32_t num_iterations)
{
	std::vector<double> times;
	for (uint32_t _i = 0u; _i < num_iterations; ++_i)
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
	for (uint32_t _i = 0u; _i < num_iterations - 2; ++_i)
	{
		avg += times[_i];
	}
	return avg / (num_iterations - 2);
}

#if HLVM_ALLOW_GPERF
	#include <gperftools/profiler.h>
#endif

int main(int ac, char* av[])
{
	{
		GExecutableName = boost::filesystem::path(TO_CHAR_CSTR(av[0])).filename().c_str();
		GExecutablePath = boost::filesystem::current_path();
	}
	{
		FGenericPlatformCrashDump::Init();
	}

	using namespace std;
	using namespace boost;
	namespace po = boost::program_options;

	try
	{
		po::options_description desc("Allowed options");
		desc.add_options()("help", "produce help message")("verbose,v", po::value<int>()->implicit_value(-1),
			"enable verbosity override to specify level")("gperf",
			po::value<int>()->implicit_value(0), "enable gerpftools profiling by cpu sample (linux only!)");

		po::store(po::command_line_parser(ac, av).options(desc).run(), GVariableMap);
		po::notify(GVariableMap);

		/**
		 * Print help
		 */
		if (GVariableMap.count("help"))
		{
			cout << "Usage: options_description [options]\n";
			cout << desc;
			return 0;
		}
		/**
		 * Change verbosity
		 */
		if (GVariableMap.count("verbose"))
		{
			GLogVerbosity = GVariableMap["verbose"].as<int>();
			cout << "options: Verbosity override enabled.  Level is " << GLogVerbosity
				 << "\n";
		}
		/**
		 * Enable gperf
		 */
		if constexpr (HLVM_ALLOW_GPERF)
		{
			if (GVariableMap.count("gperf") && GVariableMap["gperf"].as<int>() == 1)
			{
				GGperfEnabled = true;
				cout << "options: gperf enabled"
					 << "\n";
			}
		}
	}
	catch (std::exception& e)
	{
		cout << "options encountered exception:\n"
			 << e.what() << "\n";
		return 1;
	}

	// Initialize mallocator
	{
		InitMallocator();
	}
	// Initialize log redirector
	{
		FLogRedirector::Get()->AddDevice<FSpdlogConsoleDevice>();
	}
	// Run tests
	{
		HLVM_SCOPED_VARIABLE(
			Scoped,
			[&] {
				if constexpr (HLVM_ALLOW_GPERF)
				{
					if (GGperfEnabled)
					{
						ProfilerStart(FString::Format(TXT("{}_{}"), GExecutableName, TXT("gperf.prof")));
					}
				}
			},
			[&] {
				if constexpr (HLVM_ALLOW_GPERF)
				{
					if (GGperfEnabled)
					{
						ProfilerStop();
					}
				}
			});

		// Run all registered test functions
		for (auto& test_function : hlvm_private::recorded_test_functions)
		{
			test_function();
		}
	}
	// Finalize mallocator
	{
		FinlMallocator();
	}
	return 0;
}
