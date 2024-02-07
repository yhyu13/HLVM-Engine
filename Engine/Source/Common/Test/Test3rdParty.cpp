/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Test.h"

#include <ylt/struct_pack.hpp>
#include <async_simple/coro/Lazy.h>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <magic_enum_all.hpp>
#include <backward.hpp>
#include <parallel_hashmap/phmap.h>

DELCARE_LOG_CATEGORY(LogTest)
DEFINE_LOG_CATEGORY(LogTest)

/*
	<test method>
*/
RECORD(spdlog_test,
	{
		spdlog::init_thread_pool(8192, 1);
		spdlog::set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] %l: %v%$");

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
		HLVM_LOG(LogTest, info, TXT("Yalantin test"));
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
		}
		{
			auto task1 = [](int x) -> async_simple::coro::Lazy<int> {
				co_return x;
			};
			auto task2 = [&task1]() -> async_simple::coro::Lazy<> {
				auto t = task1(10);
				auto x = co_await t;
				HLVM_ASSERT(x == 10, TXT("task2 failed."));
				HLVM_LOG(LogTest, info, TXT("task2 completed successfully."));
			};
			auto func = [&task2]() -> async_simple::coro::Lazy<> {
				co_await task2();
			};
			func().start([](async_simple::Try<void> Result) {
				if (Result.hasError())
				{
					Result.value();
				}
				else
				{
					HLVM_LOG(LogTest, info, TXT("func completed successfully."));
				}
			});
		}
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

RECORD(backward_test,
	{
		using namespace backward;
		{
			StackTrace st;
			st.load_here(32);
			Printer p;
			p.print(st);
		}
	})

RECORD(phmap_test,
	{
		HLVM_LOG(LogTest, info, TXT("phmap test"));
		{
			phmap::flat_hash_map<std::string, int> map;
			map["hello"] = 1;
			map["world"] = 2;
			for (auto& [key, value] : map)
			{
				HLVM_LOG(LogTest, info, TXT("key: {} value: {}"), TO_TCHAR_STR(key.c_str()), value);
			}
			map.erase("hello");
			for (auto& [key, value] : map)
			{
				HLVM_LOG(LogTest, info, TXT("key: {} value: {}"), TO_TCHAR_STR(key.c_str()), value);
			}
			map.clear();
			for (auto& [key, value] : map)
			{
				HLVM_LOG(LogTest, info, TXT("key: {} value: {}"), TO_TCHAR_STR(key.c_str()), value);
			}
		}
		{
			phmap::node_hash_map<std::string, int> map;
			map["hello"] = 1;
			map["world"] = 2;
			for (auto& [key, value] : map)
			{
				HLVM_LOG(LogTest, info, TXT("key: {} value: {}"), TO_TCHAR_STR(key.c_str()), value);
			}
			map.erase("hello");
			for (auto& [key, value] : map)
			{
				HLVM_LOG(LogTest, info, TXT("key: {} value: {}"), TO_TCHAR_STR(key.c_str()), value);
			}
			map.clear();
			for (auto& [key, value] : map)
			{
				HLVM_LOG(LogTest, info, TXT("key: {} value: {}"), TO_TCHAR_STR(key.c_str()), value);
			}
		}
		{
			{
				phmap::parallel_flat_hash_map<std::string, int> map;
				map["hello"] = 1;
				map["world"] = 2;
				for (auto& [key, value] : map)
				{
					HLVM_LOG(LogTest, info, TXT("key: {} value: {}"), TO_TCHAR_STR(key.c_str()), value);
				}
				map.erase("hello");
				for (auto& [key, value] : map)
				{
					HLVM_LOG(LogTest, info, TXT("key: {} value: {}"), TO_TCHAR_STR(key.c_str()), value);
				}
				map.clear();
				for (auto& [key, value] : map)
				{
					HLVM_LOG(LogTest, info, TXT("key: {} value: {}"), TO_TCHAR_STR(key.c_str()), value);
				}
			}
		}
	})