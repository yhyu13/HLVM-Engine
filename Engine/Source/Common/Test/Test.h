/**
 * Copyright (c) 2026. MIT License. All rights reserved.
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
	// Test failure counter — incremented by HLVM_TEST_EXPECT* macros.
	// Reset per test, aggregated for the final summary.
	HLVM_INLINE_VAR uint32_t g_test_failure_count = 0;
	HLVM_INLINE_VAR uint32_t g_test_failure_count_total = 0;
	HLVM_INLINE_VAR uint32_t g_test_current_count = 0;
	// The current test name lives as an FString (basic_string<TCHAR>) — TCHAR is the
	// engine-wide char unit (u8 on Linux, wchar on Windows). Storing as const TCHAR*
	// matches FString::c_str().
	HLVM_INLINE_VAR FString g_test_current_name = FString(TXT("<unknown>"));
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

// =============================================================================
// HLVM_TEST_EXPECT* — real assertion primitives for tests.
// Each macro logs a structured error AND increments g_test_failure_count.
// A test that doesn't use these is silent: it reports green even when wrong.
// Use HLVM_ENSURE for *internal* invariants in production code.
//
// IMPORTANT: we pass already-stringified values to HLVM_LOG because the codebase
// uses TXT(x) which expands to u8##x via token-pasting. Pasting "::" or any
// non-string-literal token after "u8" produces invalid preprocessor output
// ("pasting formed 'u8::'"). The hlvm_private::g_test_current_name helper
// must therefore be unwrapped to its c_str() at the call site, not via TXT().
// =============================================================================
#define HLVM_TEST_FAIL_AT(cond_str, file, line, expr_str)                              \
	do                                                                                \
	{                                                                                 \
		++::hlvm_private::g_test_failure_count;                                        \
		++::hlvm_private::g_test_failure_count_total;                                 \
		::hlvm_private::g_test_current_count++;                                       \
		/* Use fmt-style {} substitution (NOT %s pointers — fmt v10+ disallows       \
		   formatting raw const char*). Pass FString values directly so fmt can       \
		   pick the string_view overload. */                                         \
		HLVM_LOG(LogTemp, critical,                                                    \
			TXT("[{}] EXPECT FAILED at {}:{}: ({}): {}"),                             \
			::hlvm_private::g_test_current_name,                                      \
			FString(TO_TCHAR_CSTR(file)),                                            \
			line,                                                                     \
			FString(TO_TCHAR_CSTR(cond_str)),                                        \
			FString(TO_TCHAR_CSTR(expr_str)));                                       \
	} while (0)

#define HLVM_TEST_EXPECT(cond)                                                        \
	do                                                                                \
	{                                                                                 \
		if (!(cond))                                                                  \
		{                                                                             \
			HLVM_TEST_FAIL_AT(#cond, __FILE__, __LINE__, "expected true");            \
		}                                                                             \
		else                                                                          \
		{                                                                             \
			::hlvm_private::g_test_current_count++;                                   \
		}                                                                             \
	} while (0)

#define HLVM_TEST_EXPECT_TRUE(x)   HLVM_TEST_EXPECT(x)
#define HLVM_TEST_EXPECT_FALSE(x)  HLVM_TEST_EXPECT(!(x))
#define HLVM_TEST_EXPECT_EQ(a, b)   HLVM_TEST_EXPECT((a) == (b))
#define HLVM_TEST_EXPECT_NE(a, b)   HLVM_TEST_EXPECT((a) != (b))
#define HLVM_TEST_EXPECT_LT(a, b)   HLVM_TEST_EXPECT((a) <  (b))
#define HLVM_TEST_EXPECT_LE(a, b)   HLVM_TEST_EXPECT((a) <= (b))
#define HLVM_TEST_EXPECT_GT(a, b)   HLVM_TEST_EXPECT((a) >  (b))
#define HLVM_TEST_EXPECT_GE(a, b)   HLVM_TEST_EXPECT((a) >= (b))

// Floating-point near-equality. eps is the max absolute difference.
#define HLVM_TEST_EXPECT_NEAR(a, b, eps)                                              \
	do                                                                                \
	{                                                                                 \
		auto _lhs = (a);                                                              \
		auto _rhs = (b);                                                              \
		auto _e   = (eps);                                                            \
		if (!(std::abs(_lhs - _rhs) <= _e))                                           \
		{                                                                             \
			HLVM_TEST_FAIL_AT("NEAR", __FILE__, __LINE__,                             \
				"expected |a - b| <= eps");                                           \
		}                                                                             \
		else                                                                          \
		{                                                                             \
			::hlvm_private::g_test_current_count++;                                   \
		}                                                                             \
	} while (0)

// Helper function to create a lambda that runs the test and prints the info
template <typename Func>
std::function<void()> _make_test_wrapper(const FString& name, Func test_function, const TestContext& ctx)
{
	return [name, test_function, ctx]() {
		if (!ctx.bEnabled)
		{
			HLVM_LOG(LogTemp, info, TXT("Skipping test {} due to it is disabled"), *name);
			return;
		}

		for (uint32_t _i = 0u; _i < ctx.repeat; ++_i)
		{
			FTimer Timer{ true };
			// Seed the random number generator
			std::srand(ctx.randomSeed);
			// Reset per-iteration counters and tag the current test for failure messages.
			hlvm_private::g_test_failure_count = 0;
			hlvm_private::g_test_current_count = 0;
			hlvm_private::g_test_current_name = name;
			HLVM_LOG(LogTemp, info, TXT("Running {} (#{})"), *name, _i + 1);
			// Run the actual test function
			// check if test_function has return type bool
			if constexpr (std::is_same_v<decltype(test_function()), bool>)
			{
				const bool ret = test_function();
				// If the function returned false OR any HLVM_TEST_EXPECT failed, mark failure.
				if (!ret || hlvm_private::g_test_failure_count > 0)
				{
					HLVM_LOG(LogTemp, critical, TXT("Test {} (#{}) FAILED: {} expect-failures, return={}"),
						*name, _i + 1, hlvm_private::g_test_failure_count, ret);
				}
			}
			else if constexpr (std::is_same_v<decltype(test_function()), int>)
			{
				int ret = test_function();
				if (ret != 0 || hlvm_private::g_test_failure_count > 0)
				{
					HLVM_LOG(LogTemp, critical, TXT("Test {} (#{}) FAILED: {} expect-failures, return={}"),
						*name, _i + 1, hlvm_private::g_test_failure_count, ret);
				}
			}
			else
			{
				test_function();
				if (hlvm_private::g_test_failure_count > 0)
				{
					HLVM_LOG(LogTemp, critical, TXT("Test {} (#{}) FAILED: {} expect-failures"),
						*name, _i + 1, hlvm_private::g_test_failure_count);
				}
			}
			HLVM_LOG(LogTemp, info, TXT("Completed {} (#{}) in {} seconds ({} assertions checked)"),
				*name, _i + 1, Timer.MarkSec(), hlvm_private::g_test_current_count);
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

#define SECTION(section_name, enabled, repeat, ...)                                                                              \
	do                                                                                                                           \
	{                                                                                                                            \
		if (enabled)                                                                                                             \
		{                                                                                                                        \
			std::function<void()> func_##section_name = [&]() {                                                                  \
				do                                                                                                               \
				{                                                                                                                \
					__VA_ARGS__                                                                                                  \
				}                                                                                                                \
				while (0);                                                                                                       \
			};                                                                                                                   \
			for (uint32_t _i = 0u; _i < repeat; ++_i)                                                                            \
			{                                                                                                                    \
				HLVM_LOG(LogTemp, info, TXT("Running section {} for {} of {} times"), TXT(#section_name), _i + 1, TXT(#repeat)); \
				func_##section_name();                                                                                           \
			}                                                                                                                    \
		}                                                                                                                        \
		else                                                                                                                     \
		{                                                                                                                        \
			HLVM_LOG(LogTemp, info, TXT("Skipping section {} due to it is disabled"), TXT(#section_name));                       \
		}                                                                                                                        \
	}                                                                                                                            \
	while (0)

// Implement smoothed average time measurement
// i.e. run test case multiple times with timer and calculate average by removing max and min
using TestFuncType = std::function<bool(double&)>;
inline double RunTestAndCalculateAvg(const TestFuncType& func, uint32_t num_iterations)
{
	std::vector<double> times;
	for (uint32_t _i = 0u; _i < num_iterations; ++_i)
	{
		double duration = std::numeric_limits<double>::max(); // Initialize duration to the maximum possible value
		HLVM_ENSURE_F(func(duration), TXT("Test case failed"));
		times.emplace_back(duration); // Store the duration in the vector
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
		const boost::filesystem::path ExecutablePath = boost::filesystem::path(TO_CHAR_CSTR(av[0]));
		GExecutableName = ExecutablePath.filename().c_str();
		GExecutablePath = boost::filesystem::current_path();
		GExecutableDirectory = ExecutablePath.has_parent_path() ? ExecutablePath.parent_path() : GExecutablePath;
#ifdef HLVM_ROOT
		GProjectRoot = TCHARSTR(LITERAL(HLVM_ROOT));
#else
		const char* hlvmRootEnv = std::getenv("HLVM_ROOT");
		if (hlvmRootEnv != nullptr)
		{
			GProjectRoot = boost::filesystem::canonical(hlvmRootEnv);
		}
#endif
		if (!boost::filesystem::exists(GProjectRoot))
		{
			std::cout << "HLVM_ROOT environment variable is not set or invalid" << std::endl;
		}
		else
		{
			std::cout << "Project root: " << GProjectRoot << std::endl;
		}
		// Print other paths
		if (!boost::filesystem::exists(GExecutablePath))
		{
			std::cout << "GExecutablePath invalid" << std::endl;
		}
		else
		{
			std::cout << "GExecutablePath: " << GExecutablePath << std::endl;
		}
		if (!boost::filesystem::exists(GExecutableDirectory))
		{
			std::cout << "GExecutableDirectory invalid" << std::endl;
		}
		else
		{
			std::cout << "GExecutableDirectory: " << GExecutableDirectory << std::endl;
		}
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
		desc.add_options()("help", "produce help message")("v-lvl", po::value<int>()->implicit_value(-1), "enable verbosity override to specify level")("gperf", po::value<int>()->implicit_value(0), "enable gerpftools profiling by cpu sample (linux only!)")("no-cpu-profile", po::value<int>()->implicit_value(0), "disable cpu profiling (tracy)");

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
		if (GVariableMap.count("v-lvl"))
		{
			GLogVerbosity = GVariableMap["v-lvl"].as<int>();
			cout << "options: verbosity override is " << GLogVerbosity
				 << "\n";
		}

#if HLVM_ALLOW_GPERF
		/**
		 * Enable gperf
		 */
		if (GVariableMap.count("gperf") && GVariableMap["gperf"].as<int>() == 1)
		{
			GGperfEnabled = true;
			cout << "options: gperf 1"
				 << "\n";
		}
#endif
#if HLVM_PROFILER_COMPILE
		/**
		 * Enable cpu profiler by default
		 */
		if (GVariableMap.count("no-cpu-profile") && GVariableMap["no-cpu-profile"].as<int>() == 1)
		{
			FProfilerCPU::bEnabled = false;
			cout << "options: no-cpu-profile 1"
				 << "\n";
		}
#endif
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
		FLogRedirector::Get()->AddDevice<FSpdlogFileDevice>();
	}
	// Run tests
	{
		HLVM_SCOPED_VARIABLE(
			Scoped,
			[&] {
				HLVM_PROFILER_EXEC(FProfilerCPU::OnFrameBegin());
				HLVM_ALLOW_GPERF_EXEC(
					if (GGperfEnabled) {
						ProfilerStart(FString::Format(TXT("{}_{}"), GExecutableName, TXT("gperf.prof")));
					});
			},
			[&] {
				HLVM_ALLOW_GPERF_EXEC(
					if (GGperfEnabled) {
						ProfilerStop();
					});
				HLVM_PROFILER_EXEC(FProfilerCPU::OnFrameEnd());
				HLVM_PROFILER_EXEC(FProfilerCPU::Dispose());
			});

		// Run all registered test functions
		for (auto& test_function : hlvm_private::recorded_test_functions)
		{
			test_function();
		}
	}

	// Final summary — fail the process if any assertion failed.
	if (hlvm_private::g_test_failure_count_total > 0)
	{
		HLVM_LOG(LogTemp, critical, TXT("=== TEST SUITE FAILED: {} total assertion failures across the run ==="),
			hlvm_private::g_test_failure_count_total);
		std::cout << "TEST SUITE FAILED: " << hlvm_private::g_test_failure_count_total
			<< " assertion failures" << std::endl;
		// Still call mallocator finalize to keep the leak checker happy,
		// but return non-zero so CI / scripts can detect failure.
		FinlMallocator();
		return 1;
	}
	else
	{
		HLVM_LOG(LogTemp, info, TXT("=== TEST SUITE PASSED ==="));
	}

	// Finalize mallocator
	{
		FinlMallocator();
	}

	//	tracy::GetProfiler().RequestShutdown();
	//	std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
	//	while( !tracy::GetProfiler().HasShutdownFinished() )
	//	{
	//		HLVM_LOG(LogTemp, info, TXT("Waiting for tracy to shutdown..."));
	//		std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
	//	}

	return 0;
}
