/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Log.h"
#include "Core/String.h"
#include "Core/Assert.h"
#include "Utility/Timer.h"
#include "Core/Mallocator/IMallocator.h"
#include "Platform/GenericPlatformCrashDump.h"

#include <iostream>
#include <vector>
#include <functional>
#include <chrono>

namespace hlvm_private
{
	HLVM_INLINE_VAR std::vector<std::function<void()>> recorded_test_functions{};
}

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

/**
 * AutoRegisterContext
 *  bEnabled is used to enable/disable the auto registration of test functions.
 */
struct AutoRegisterContext
{
	bool bEnabled = true;
};

#define RECORD_TEST_FUNC_BODY(test_function)                                                                              \
	struct AutoRegister                                                                                                   \
	{                                                                                                                     \
		AutoRegister()                                                                                                    \
		{                                                                                                                 \
			if (_AutoRegisterContext.bEnabled)                                                                            \
				hlvm_private::recorded_test_functions.push_back(make_test_wrapper(#test_function, test_##test_function)); \
		}                                                                                                                 \
	};                                                                                                                    \
	static AutoRegister _AutoRegister

#define RECORD_TEST_FUNC1(test_function, ...)                         \
	HLVM_STATIC_VAR AutoRegisterContext _AutoRegisterContext{ true }; \
	RECORD_TEST_FUNC_BODY(test_function);

#define RECORD_TEST_FUNC2(test_function, ...)                                \
	HLVM_STATIC_VAR AutoRegisterContext _AutoRegisterContext{ __VA_ARGS__ }; \
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
				if constexpr (ArgCount == 2)                                          \
				{                                                                     \
					RECORD_TEST_FUNC2(test_function, ##__VA_ARGS__);                  \
				}                                                                     \
			}                                                                         \
		};                                                                            \
		static RecordTestFunc _RecordTestFunc;                                        \
	}

/**
 * Macro to record a test function (easier version)
 * Example :
 * RECORD(hash_test, true)
 * {
 *    ...
 * }
 */
#define RECORD(test_function, ...)                  \
	void test_##test_function();                    \
	RECORD_TEST_FUNC(test_function, ##__VA_ARGS__); \
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

#include <boost/program_options.hpp>

#if HLVM_ALLOW_GPERF
	#include <gperftools/profiler.h>
#endif

int main(int ac, char* av[])
{
	FrameMarkNamed("main");
	{
		GExecutableName = TO_TCHAR_STR(av[0]);
	}
	{
		FGenericPlatformCrashDump::Init();
	}

	using namespace std;
	using namespace boost;
	namespace po = boost::program_options;

	try
	{
		po::variables_map		vm;
		po::options_description desc("Allowed options");
		desc.add_options()("help", "produce help message")("verbose,v", po::value<int>()->implicit_value(-1),
			"enable verbosity override to specify level")("gperf",
			po::value<int>()->implicit_value(0), "enable gerpftools profiling by cpu sample (linux only!)");

		po::store(po::command_line_parser(ac, av).options(desc).run(), vm);
		po::notify(vm);

		/**
		 * Print help
		 */
		if (vm.count("help"))
		{
			cout << "Usage: options_description [options]\n";
			cout << desc;
			return 0;
		}
		/**
		 * Change verbosity
		 */
		if (vm.count("verbose"))
		{
			GVerbosity = vm["verbose"].as<int>();
			cout << "options: Verbosity override enabled.  Level is " << GVerbosity
				 << "\n";
		}
		/**
		 * Enable gperf
		 */
		if constexpr (HLVM_ALLOW_GPERF)
		{
			if (vm.count("gperf") && vm["gperf"].as<int>() == 1)
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

	return 0;
}
