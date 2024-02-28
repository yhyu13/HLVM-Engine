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

		auto all_matches = FPath::Find("./", R"(.*Test.*)", true);
		HLVM_LOG(LogTest, info, TXT("Test FPath::Find result:\n{}"), *FPath::DumpJson(all_matches));
	}
}
RECORD_TEST_FUNC(boostfile_test)

RECORD(packed_test)
{
	HLVM_LOG(LogTest, info, TXT("Test PackedFileHandle"));

	{
		FPath PackedFileName = "./packed-test";
		FPath PackedTokFile = PackedFileName.ChangeExtension(HLVM_PACKED_TOKEN_EXT);
		FPath PackedCotFile = PackedFileName.ChangeExtension(HLVM_PACKED_CONTAINER_EXT);
		HLVM_LOG(LogTest, info, TXT("Test PackedFileHandle write token file: {}"), *PackedTokFile);
		{
			FBoostFileHandle fileTokHandle, fileCotHandle;
			FFileOptions	 Options{ .eFileMode = EFileMode::WB, .eFileMapped = EFileMapped::Mapped, .eFileLock = EFileLock::InterProcessLock };
			HLVM_SCOPED_VARIABLE(
				ScopedFileHandle, [&]() -> void {
                    fileTokHandle.Open(PackedTokFile, Options);
                    fileCotHandle.Open(PackedCotFile, Options); },
				[&]() -> void {
					fileTokHandle.Close();
					fileCotHandle.Close();
				});

			constexpr size_t										   size = 8;
			TVector<std::tuple<FPackedTokenEntry, TVector<std::byte>>> PackedData;
			PackedData.resize(size);
			size_t StartPos = 0;
			for (size_t i = 0; i < size; ++i)
			{
				auto& Entry = get<0>(PackedData[i]);
				Entry.Data.EncryptType = EEncryptType::No;
				Entry.Data.CompressType = ECompressType::No;
				auto& ContentBuffer = get<1>(PackedData[i]);
				// Dummy content, should do some post-processing based on compress and encrypt type in production code
				ContentBuffer.resize(8);

				Entry.PathHash = FPath{ FString::Format(TXT("test_{}.txt"), i), EPlatformFileType::Packed };
				Entry.Data.StartPos = StartPos;
				Entry.Data.Size = ContentBuffer.size();
				Entry.Data.DecompressSize = ContentBuffer.size();

				StartPos += S_C(size_t, Entry.Data.Size);
			}

			TVector<std::byte> TokenData;
			TVector<std::byte> CotData;
			for (size_t i = 0; i < size; ++i)
			{
				const auto& Entry = get<0>(PackedData[i]);
				const auto& ContentBuffer = get<1>(PackedData[i]);
				{
					std::byte			 buffer[FPackedTokenEntry_SerializedSize];
					std::span<std::byte> buffer_span = buffer;
					bool				 bSuccess = GetSerialized(Entry, buffer_span);
					HLVM_ENSURE(bSuccess, TXT("GetSerialized failed"));
					std::move(buffer_span.begin(), buffer_span.end(), std::back_inserter(TokenData));
				}
				{
					std::move(ContentBuffer.begin(), ContentBuffer.end(), std::back_inserter(CotData));
				}
			}

			// Compress and Encrypt
			{
				auto Compressed = FZstd::Compress(TokenData);
				auto Encrypted = FRSA::EncryptPCKS8(Compressed);
				fileTokHandle.Write(Encrypted.data(), Encrypted.size());
			}
			{
				fileCotHandle.Write(CotData.data(), CotData.size());
			}
		}

		{
			HLVM_LOG(LogTest, info, TXT("Test PackedFileHandle read token file: {}"), *PackedTokFile);
			FPackedFileHandle fileHandle;
			HLVM_SCOPED_VARIABLE(
				ScopedFileHandle, [&]() -> void { fileHandle.Open(PackedFileName); },
				[&]() -> void { fileHandle.Close(); });
		}
	}
}
