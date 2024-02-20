/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Test.h"

#include "Platform/FileSystem/Boost/BoostFileHandle.h"

DELCARE_LOG_CATEGORY(LogTest)
DEFINE_LOG_CATEGORY(LogTest)

/*
	<test method>
*/
static void test_boostfile_test()
{
	HLVM_LOG(LogTest, info, TXT("Test BoostFileHandle"));
	{
		FBoostFileHandle fileHandle;
		FFileOptions	 Options{ .eFileMode = EFileMode::RW, .eFileMapped = EFileMapped::NoMapped };
		FFileOpStatus	 Status;
		TCharArrayStr<4> Buffer;
		fileHandle.Open(TXT("./test.txt"), Options, &Status)
			.Write("test", 4, { .EraseSeekPos = true })
			.Read(Buffer.data(), Buffer.Capacity, { .EraseSeekPos = true })
			.Write("asdf", 4, { .Offset = 4, .Whence = EWhence::Begin, .EraseSeekPos = true })
			.Read(Buffer.data(), Buffer.Capacity, { .Offset = 4, .Whence = EWhence::Begin, .EraseSeekPos = true })
			.Close(&Status);
		HLVM_LOG(LogTest, info, TXT("Test BoostFileHandle nomap result: {}"), Buffer.c_str());
	}
	{
		/**
		 * Mapped file cannot be create by file open, so only read write
		 */
		FBoostFileHandle fileHandle;
		FFileOptions	 Options{ .eFileMode = EFileMode::RW, .eFileMapped = EFileMapped::Mapped };
		FFileOpStatus	 Status;
		TCharArrayStr<4> Buffer;
		fileHandle.Open(TXT("./test_mapped.txt"), Options, &Status)
			.Write("test", 4, { .EraseSeekPos = true })
			.Read(Buffer.data(), Buffer.Capacity, { .EraseSeekPos = true })
			.Write("asdf", 4, { .Offset = 4, .Whence = EWhence::Begin, .EraseSeekPos = true })
			.Read(Buffer.data(), Buffer.Capacity, { .Offset = 4, .Whence = EWhence::Begin, .EraseSeekPos = true })
			.Close(&Status);
		HLVM_LOG(LogTest, info, TXT("Test BoostFileHandle mapped result: {}"), Buffer.c_str());
	}
	{
		HLVM_ENSURE(!FPath::IsDirectory("./test.txt"), TXT("Test failed"));
		HLVM_ENSURE(FPath::Exists("./test.txt"), TXT("Test failed"));

		auto all_matches = FPath::FindAllMatch("./", ".*Test.*", true);
		HLVM_LOG(LogTest, info, TXT("Test FPath::FindAllMatch result:\n{}"), *FPath::DumpJson(all_matches));
	}
}
RECORD_TEST_FUNC(boostfile_test)
