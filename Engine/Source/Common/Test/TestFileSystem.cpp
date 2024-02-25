/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Test.h"

#include "Platform/FileSystem/Boost/BoostFileHandle.h"
#include "Platform/FileSystem/Packed/PackedFileHandle.h"

#include <ylt/struct_pack.hpp>
#include <ylt/struct_json/json_reader.h>
#include <ylt/struct_json/json_writer.h>

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
		FFileOptions	 Options{ .eFileMode = EFileMode::RW, .eFileMapped = EFileMapped::NoMapped, .eFileLock = EFileLock::FullLock };
		FFileOpStatus	 Status;
		TCharArrayStr<4> Buffer;
		fileHandle.Open(TXT("./test.txt"), Options)
			.Write("test", 4, { .bEraseSeekPos = true })
			.Read(Buffer.data(), Buffer.Capacity, { .bEraseSeekPos = true })
			.Write("asdf", 4, { .Offset = 4, .Whence = EWhence::Begin, .bEraseSeekPos = true })
			.Read(Buffer.data(), Buffer.Capacity, { .Offset = 4, .Whence = EWhence::Begin, .bEraseSeekPos = true })
			.Close();
		HLVM_LOG(LogTest, info, TXT("Test BoostFileHandle nomap result: {}"), Buffer.c_str());
	}
	{
		/**
		 * Mapped file cannot be create by file open, so only read write
		 */
		FBoostFileHandle fileHandle;
		FFileOptions	 Options{ .eFileMode = EFileMode::RW, .eFileMapped = EFileMapped::Mapped, .eFileLock = EFileLock::FullLock };
		TCharArrayStr<4> Buffer;
		fileHandle.Open(TXT("./test_mapped.txt"), Options)
			.Write("test", 4, { .bEraseSeekPos = true })
			.Read(Buffer.data(), Buffer.Capacity, { .bEraseSeekPos = true })
			.Write("asdf", 4, { .Offset = 4, .Whence = EWhence::Begin, .bEraseSeekPos = true })
			.Read(Buffer.data(), Buffer.Capacity, { .Offset = 4, .Whence = EWhence::Begin, .bEraseSeekPos = true })
			.Close();
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

RECORD(packed_test)
{
	HLVM_LOG(LogTest, info, TXT("Test PackedFileHandle"));

	{
		FPath PackedFileName = "./packed-test";
		FPath PackedTokFile = PackedFileName.ChangeExtension(HLVM_PACKED_TOKEN_EXT);
		{
			FBoostFileHandle fileHandle;
			FFileOptions	 Options{ .eFileMode = EFileMode::WB, .eFileMapped = EFileMapped::Mapped, .eFileLock = EFileLock::InterProcessLock };
			HLVM_SCOPED_VARIABLE(
				ScopedFileHandle, void(), [&]() -> void { fileHandle.Open(PackedTokFile, Options); },
				[&]() -> void { fileHandle.Close(); });

			TCharArrayStr<4, TCHAR> Buffer{ HLVM_JSONL_LINE_SEPARATOR };

			constexpr size_t size = 8;
			for (size_t i = 0; i < size; ++i)
			{
				FPackedTokenEntryData Entry;
				Entry.StartPos = i * 8;
				Entry.Size = 8;
				Entry.DecompressSize = 8;
				Entry.EncryptType = EEncryptType::No;
				Entry.CompressType = ECompressType::No;

				FPackedTokenEntry Sample;
				Sample.Path = FString::Format(TXT("test_{}.txt"), i);
				Sample.Entry = MoveTemp(Entry);

				auto buffer = struct_pack::serialize(Sample);
				fileHandle.Write(buffer.data(), buffer.size());
				fileHandle.Write(Buffer.data(), Buffer.Size);
			}
		}

		{
			FPackedFileHandle fileHandle;
			HLVM_SCOPED_VARIABLE(
				ScopedFileHandle, void(), [&]() -> void { fileHandle.Open(PackedFileName); },
				[&]() -> void { fileHandle.Close(); });
		}
	}
}
