/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Test.h"

#include "Platform/FileSystem/Boost/BoostFileHandle.h"

DELCARE_LOG_CATEGORY(LogTest)
DEFINE_LOG_CATEGORY(LogTest)

template <size_t N>
struct CharArrayString
{
	static constexpr size_t Capacity{ N };
	char					Buffer[Capacity + 1];
	size_t					Size{ 0 };

	CharArrayString()
	{
		std::memset(Buffer, 0, Capacity + 1);
	}

	// 构造函数接受一个C风格字符串进行初始化
	CharArrayString(const char* input)
	{
		Size = std::strlen(input);
		assert(Size <= Capacity);
		std::strncpy(Buffer, input, Size);
		Buffer[Size + 1] = '\0'; // 确保总是以空字符结束
	}

	// 获取字符串内容
	const char* c_str() const
	{
		return Buffer;
	}

	char* data()
	{
		return Buffer;
	}
};

/*
	<test method>
*/
static void test_boostfile_test()
{
	HLVM_LOG(LogTest, info, TXT("Test BoostFileHandle"));
	{
		std::shared_ptr<IFileHandle> fileHandle = std::make_shared<FBoostFileHandle>();
		FFileOptions				 Options{ .eFileMode = EFileMode::RW, .eFileMapped = EFileMapped::NoMapped };
		FFileOpStatus				 Status;
		CharArrayString<4>			 Buffer;
		fileHandle->Open(TXT("./test.txt"), Options, &Status)
			.Write("test", 4)
			.Read(Buffer.data(), Buffer.Capacity)
			.Write("asdf", 4, { .Offset = 4, .Whence = EWhence::Begin })
			.Read(Buffer.data(), Buffer.Capacity, { .Offset = 4, .Whence = EWhence::Begin })
			.Close(&Status);
		HLVM_LOG(LogTest, info, TXT("Test BoostFileHandle nomap result: {}"), TO_TCHAR_STR(Buffer.c_str()));
	}
	{
		/**
		 * Mapped file cannot be create by file open, so only read write
		 */
		std::shared_ptr<IFileHandle> fileHandle = std::make_shared<FBoostFileHandle>();
		FFileOptions				 Options{ .eFileMode = EFileMode::RW, .eFileMapped = EFileMapped::Mapped };
		FFileOpStatus				 Status;
		CharArrayString<4>			 Buffer;
		fileHandle->Open(TXT("./test_mapped.txt"), Options, &Status)
			.Write("test", 4)
			.Read(Buffer.data(), Buffer.Capacity)
			.Write("asdf", 4, { .Offset = 4, .Whence = EWhence::Begin })
			.Read(Buffer.data(), Buffer.Capacity, { .Offset = 4, .Whence = EWhence::Begin })
			.Close(&Status);
		HLVM_LOG(LogTest, info, TXT("Test BoostFileHandle mapped result: {}"), TO_TCHAR_STR(Buffer.c_str()));
	}
	{
		HLVM_ENSURE(!FPath::IsDirectory("./test.txt"), TXT("Test failed"));
		HLVM_ENSURE(FPath::Exists("./test.txt"), TXT("Test failed"));

		auto all_matches = FPath::FindAllMatch("./", ".*Test.*", true);
		auto all_matches_str = FString::Join(all_matches, [](auto& item) { return FString::Format(TXT("\"{}\""), *item); });
		HLVM_LOG(LogTest, info, TXT("Test FPath::FindAllMatch result:\n{}"), all_matches_str);
	}
}
RECORD_TEST_FUNC(boostfile_test)
