#include "Common.h"

#include <ylt/struct_pack.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <magic_enum_all.hpp>

/*
	<test suit>
*/
#include <iostream>
#include <vector>
#include <functional>
#include <chrono>

std::vector<std::function<void()>> recorded_test_functions;

// Helper function to create a lambda that runs the test and prints the info
template <typename Func>
std::function<void()> make_test_wrapper(const std::string& name, Func test_function)
{
	return [name, test_function]() {
		std::cout << "Running " << name << "..." << std::endl;
		auto start = std::chrono::high_resolution_clock::now();

		test_function(); // Run the actual test function

		auto						  end = std::chrono::high_resolution_clock::now();
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

/*
	<test method>
*/
RECORD(spdlog_test,
	{
		// spdlog::init_thread_pool(8192, 2);
		spdlog::set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] %n:%l: %v%$");

		spdlog::info("Welcome to spdlog!");
		spdlog::error("Some error message with arg: {}", 1);

		spdlog::warn("Easy padding in numbers like {:08d}", 12);
		spdlog::critical("Support for int: {0:d};  hex: {0:x};  oct: {0:o}; bin: {0:b}", 42);
		spdlog::info("Support for floats {:03.2f}", 1.23456);
		spdlog::info("Positional args are {1} {0}..", "too", "supported");
		spdlog::info("中文 is {0}{1}..", "也", "支持的");
		std::u8string str = u8"u8中文";
		spdlog::info((char*)str.data());
		spdlog::info("{:<30}", "left aligned");

		spdlog::set_level(spdlog::level::debug); // Set global log level to debug
		spdlog::debug("This message should be displayed..");

		// change log pattern
		spdlog::set_pattern("[%H:%M:%S %z] [%n] [%^---%L---%$] [thread %t] %v");

		// Compile time log levels
		// Note that this does not change the current log level, it will only
		// remove (depending on SPDLOG_ACTIVE_LEVEL) the call on the release code.
		SPDLOG_TRACE("Some trace message with param {}", 42);
		SPDLOG_DEBUG("Some debug message");
	})

RECORD(yalantinlibs_test,
	{
		// Yalantin example
		struct person
		{
			int64_t		id;
			std::string name;
			int			age;
			double		salary;

			bool operator==(const person& other) const
			{
				return id == other.id && name == other.name && age == other.age && salary == other.salary;
			}
		};

		person person1{ .id = 1, .name = "hello struct pack", .age = 20, .salary = 1024.42 };

		// one line code serialize
		auto buffer = struct_pack::serialize(person1);

		// one line code deserialization
		person person2;
		auto   ec = struct_pack::deserialize_to(person2, buffer.data(), buffer.size());
		assert(!ec);
		assert(person1 == person2);
	})

RECORD(magic_enum_test,
	{
		enum class Color : int
		{
			NONE = -1,
			RED,
			GREEN,
			BLUE
		};
		{
			Color color = Color::RED;
			auto  color_name = magic_enum::enum_name(color);
			// color_name -> "RED"
		}
		{
			std::string color_name{ "GREEN" };
			auto		color = magic_enum::enum_cast<Color>(color_name);
			if (color.has_value())
			{
				// color.value() -> Color::GREEN
			}

			// case insensitive enum_cast
			auto color2 = magic_enum::enum_cast<Color>(color_name, magic_enum::case_insensitive);

			// enum_cast with BinaryPredicate
			auto color3 = magic_enum::enum_cast<Color>(color_name, [](char lhs, char rhs) { return std::tolower(lhs) == std::tolower(rhs); });

			// enum_cast with default
			auto color_or_default = magic_enum::enum_cast<Color>(color_name).value_or(Color::NONE);
		}

		{
			int	 color_integer = 2;
			auto color = magic_enum::enum_cast<Color>(color_integer);
			if (color.has_value())
			{
				// color.value() -> Color::BLUE
			}

			auto color_or_default = magic_enum::enum_cast<Color>(color_integer).value_or(Color::NONE);
		}
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