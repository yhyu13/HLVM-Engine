/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include <iostream>
#include <vector>
#include <functional>
#include <chrono>

static std::vector<std::function<void()>> recorded_test_functions;

// Helper function to create a lambda that runs the test and prints the info
template <typename Func>
std::function<void()> make_test_wrapper(const std::string& name, Func test_function)
{
	return [name, test_function]() {
		std::cout << "Running " << name << "..." << std::endl;
		auto start = std::chrono::steady_clock::now();

		test_function(); // Run the actual test function

		auto						  end = std::chrono::steady_clock::now();
		std::chrono::duration<double> elapsed = end - start;
		std::cout << "Completed " << name << ". Time elapsed: " << elapsed.count() << " seconds" << std::endl;
	};
}

// Macro to record a test function
#define RECORD(test_function, ...)                                                                          \
	void test_##test_function()                                                                             \
	{                                                                                                       \
		__VA_ARGS__;                                                                                        \
	};                                                                                                      \
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

int main()
{
	// Run all registered test functions
	for (auto& test_function : recorded_test_functions)
	{
		test_function();
	}
	return 0;
}