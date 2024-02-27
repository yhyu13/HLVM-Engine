/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Test.h"

#include <ylt/struct_pack.hpp>
#include <ylt/struct_json/json_reader.h>
#include <ylt/struct_json/json_writer.h>
#include <ylt/thirdparty/async_simple/coro/Lazy.h>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <magic_enum_all.hpp>
#include <backward.hpp>
#include <ctre.hpp>

/**
 * phmap has alot of unconventional warnings, pretty bad code though
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcomma"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#include <parallel_hashmap/phmap.h>
#pragma clang diagnostic pop

/**
 * Baton
 */
#include <botan/auto_rng.h>
#include <botan/hex.h>
#include <botan/pk_keys.h>
#include <botan/pkcs8.h>
#include <botan/pubkey.h>
#include <botan/rng.h>

// zstd
#include <zstd.h>

//// Advobfuscator
// #include <Lib/MetaString.h>

DELCARE_LOG_CATEGORY(LogTest)
DEFINE_LOG_CATEGORY(LogTest)

/*
	<test method>
*/
RECORD(spdlog_test)
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
	spdlog::info(reinterpret_cast<const char*>(str.c_str()));
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
};

struct json_person
{
	std::string name;
	int			age;

	bool operator==(const json_person& other) const
	{
		return name == other.name && age == other.age;
	}
};
REFLECTION(json_person, name, age);

RECORD(yalantinlibs_test)
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
		json_person p{ .name = "tom", .age = 20 };
		std::string str;
		struct_json::to_json(p, str); // {"name":"tom","age":20}

		json_person p1;
		struct_json::from_json(p1, str);

		assert(p == p1);
	}
	{
		auto task1 = [](int x) -> async_simple::coro::Lazy<int> {
			co_return x;
		};
		auto task2 = [&task1]() -> async_simple::coro::Lazy<> {
			auto t = task1(10);
			auto x = co_await t;
			HLVM_ENSURE(x == 10, TXT("task2 failed."));
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
};

RECORD(magic_enum_test)
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
};

RECORD(backward_test)
{
	using namespace backward;
	{
		StackTrace st;
		st.load_here(32);
		Printer p;
		p.print(st);
	}
};

RECORD(phmap_test)
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
};

constexpr auto match(std::string_view sv) noexcept
{
	return ctre::match<"h.*">(sv);
};

constexpr auto match_functionCall(std::string_view sv) noexcept
{
	return ctre::match<R"(\w+\((.*?)\)\w*)">(sv);
};

constexpr auto match_assignment(std::string_view sv) noexcept
{
	return ctre::match<R"(.*?[^=]=[^=].*?)">(sv);
};

RECORD(test_ctre)
{
	HLVM_ENSURE(match("h.h.cpp"), TXT("Failed"));
	HLVM_ENSURE(match_functionCall("a()"), TXT("Failed"));
	HLVM_ENSURE(!match_functionCall("(a)"), TXT("Failed"));
	HLVM_ENSURE(match_assignment("a() = 1"), TXT("Failed"));
	HLVM_ENSURE(!match_assignment("bool(a == 1)"), TXT("Failed"));
};

RECORD(test_botan)
{
	std::string plaintext(
		"Your great-grandfather gave this watch to your granddad for good luck. "
		"Unfortunately, Dane's luck wasn't as good as his old man's.");
	std::vector<uint8_t>  pt(plaintext.data(), plaintext.data() + plaintext.length());
	Botan::AutoSeeded_RNG rng;

	const char* pk = OBFUSCATED_LONG("-----BEGIN PRIVATE KEY-----\n"
									 "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCcky+A7QIaNdnj\n"
									 "Vfz7p61+vdbgYegcqBFuxB9LhS9SIPiTMRd5QTbmW3noLoh7nf2zOorYpgGA9s4Q\n"
									 "oH1iRJt2hJFmwHjchzkWS+m9dpU573L4fXkFcEVKDX2Hcox53xMriLGwph/IIhTM\n"
									 "B0UOTD9a3soMP2dy6ca4SIzS2MQLKhEJ9W4J7DDb3X/BgwLyL853tIXTXMXEaK01\n"
									 "5rA/D0vOBOVYLuoyGvTejG+dJtjC645qyW2QAH8O9eeiHpnrUWlLEB0rbSuMtLLc\n"
									 "nqpNv3mMGJSXPVH+Z7bvp/DCtSHGFdvPqJHmHDowoELySCJWBo9rzjbMG7yWbZhu\n"
									 "hwJ1O5HVAgMBAAECggEALhc1uKIMKGMJzN4XJo1piRGhG1Q225it6QlanQSLtYV3\n"
									 "Jv0gh9UmsBDlhe+MxbbwY74r+TKE5s3AQyy7PB4uFWlY1AJ8kY5Mw799AwTRUMin\n"
									 "83YcFfnCq9XkyeZya727CummRPXiDSvbK2RIc7kGPV2v5IMVlMI0eTMCIV8l9pLp\n"
									 "cwdX6sCVsgowi/WikPpc4kqO1eN31h/yyPRgRiZfl8x5eNhBMXWA89rDeVudhOG1\n"
									 "hGuggsa4HRhL1BMhhZgiTCXGXg0yNuH4PdNsu/6bxi8HU7VuLuCJvXyMi3/F2E4O\n"
									 "LQYw8dJXAsKiFRDRZhMCNEX7VoZnRhxSHRzAkpLQ0QKBgQDUqzvEDx+xlCSMNV5V\n"
									 "JdQ7+MCHW9CPpHTxY6dieXCLKu8eujPySyKRb6Lu7rKLRpTUqM3C+BdYzEIO/3ch\n"
									 "SDp6TU1S/YDX6Ns1YVoECUeHW16JYsgFSCIIaPVKI/sDtLnxKO0L++1ES/Q/EzI5\n"
									 "/sOmbhMhE6RRKbTCyeU7aZCcmwKBgQC8ehp+euzDZhXOJrvC45gOvsLvHwjRaa6B\n"
									 "sBNWjK8vEMxnYDn/yctK1vNiXyqXQkfU5+tb2Gl0xFDlDQnOMNdz2CB+cB5S18Il\n"
									 "o3QRcRBJLSIclBi0bJ2ShtOjmAjsA2aNdPCF3I03v+thWPg0JwHU4jvA5MOrlyMz\n"
									 "rhhVaZKaTwKBgQC+/FP8+3QD/r1fqOHhZ8qUXQ1RwfOnvBJFYbBFcDfQ0yrRFnlm\n"
									 "I3GU2IOjPXvcNfOck0fNywz0cuL0IxnyKrJReTBy0jQuMfECnD3BmC/DcBuTF8H3\n"
									 "dFMT6GY4Qd2/80J1P8K523G1vtxG9M9LY+6CfSHyt3f+Z4zszXFujJwQ9wKBgGjh\n"
									 "dZ7OXrQ5OYcXLMK9jZ8e7jDMT0bhDTejuI8gCFgje+tGs9+v9k12Iceq93Nmcbx7\n"
									 "NhBM9BoDKJTdVYiEy0/ug954G5ez8pipRWxzQ0HFOMc4birwihrApkLR1p0nI0ky\n"
									 "Oqny2i6cwKnSLYQv9Kf3IJMteekhWHhot6fH8MmnAoGBALj74j42t5724DORgBUS\n"
									 "oqRIYup7MBxjM9XQrGghmJHV1L0dSSbT/D1zxQn5Epfy7Ua16+SdBcCMLDAAyRSK\n"
									 "bYdnR7mpTKNOezSFqeJ1ZluwR1zyBXyrFdS0N1kosKA3VobVXy0SDOXATk2x3Hub\n"
									 "fAHoeqUUsEqkp4lJIXvMrw0/\n"
									 "-----END PRIVATE KEY-----");

	// load keypair
	Botan::secure_vector<uint8_t> in{ pk, pk + std::strlen(pk) };
	auto						  kp = Botan::PKCS8::load_key(in);

	// encrypt with pk
	Botan::PK_Encryptor_EME enc(*kp, rng, "OAEP(SHA-256)");
	std::vector<uint8_t>	ct = enc.encrypt(pt, rng);

	// decrypt with sk
	Botan::PK_Decryptor_EME		  dec(*kp, rng, "OAEP(SHA-256)");
	Botan::secure_vector<uint8_t> pt2 = dec.decrypt(ct);
	const char*					  pt2_str = R_C(const char*, pt2.data());
	assert(strcmp(pt2_str, plaintext.c_str()) == 0);

	std::cout << "\nenc: " << Botan::hex_encode(ct) << "\ndec: " << pt2_str << std::endl;
};

RECORD(test_zstd)
{
	std::string		  input = "This is a test string to compress.";
	std::vector<char> compressed(ZSTD_compressBound(input.size()));
	size_t			  compressedSize = ZSTD_compress(compressed.data(), compressed.size(), input.data(), input.size(), 1);

	if (ZSTD_isError(compressedSize))
	{
		std::cerr << "Compression error: " << ZSTD_getErrorName(compressedSize) << std::endl;
		assert(false);
	}

	compressed.resize(compressedSize);

	std::vector<char> decompressed(input.size());
	size_t			  decompressedSize = ZSTD_decompress(decompressed.data(), decompressed.size(), compressed.data(), compressedSize);

	if (ZSTD_isError(decompressedSize))
	{
		std::cerr << "Decompression error: " << ZSTD_getErrorName(decompressedSize) << std::endl;
		assert(false);
	}

	decompressed.resize(decompressedSize);

	std::string output(decompressed.begin(), decompressed.end());
	std::cout << "Original: " << input << std::endl;
	std::cout << "Compressed: ";
	for (char c : compressed)
	{
		std::cout << std::hex << static_cast<int>(c) << ' ';
	}
	std::cout << std::endl;
	std::cout << "Decompressed: " << output << std::endl;
	assert(input == output);
}
