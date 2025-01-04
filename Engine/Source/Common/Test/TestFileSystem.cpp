/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Test.h"

#include "Core/Mallocator/PMR.h"
#include "Platform/FileSystem/Boost/BoostPlatformFile.h"
#include "Platform/FileSystem/Packed/PackedPlatformFile.h"
#include "Core/Parallel/Async/Async.h"

// #include <ylt/struct_pack.hpp>
// #include <ylt/struct_json/json_reader.h>
// #include <ylt/struct_json/json_writer.h>

DECLARE_LOG_CATEGORY(LogTest)

#define TEST_STACK_ALLOCATOR 0
#define TEST_FIBER_POOL 0

/*
	<test method>
*/
RECORD(boostfile_test, true, 1, 1)
{
#if TEST_STACK_ALLOCATOR
	//  Try use StackMallocator and you will have lifetime object crash on free
	TStackMallocator<> StackMallocator{};
	HLVM_SCOPED_VARIABLE(
		ScopedMallocator, [&]() -> void { SwapMallocator(&StackMallocator); },
		[&]() -> void { SwapMallocator(); });
#else
	FMiMallocator MiMallocator{ { .bNewHeap = true } };
	HLVM_SCOPED_VARIABLE(
		ScopedMallocator, [&]() -> void { SwapMallocator(&MiMallocator); },
		[&]() -> void { SwapMallocator(); });
#endif

	HLVM_LOG(LogTest, info, TXT("Test BoostFileHandle"));
	{
		FBoostStreamFileHandle fileHandle;
		FFileOptions		   Options{ .eFileMode = EFileMode::RW, .eFileMapped = EFileMapped::NoMapped, .eFileLock = EFileLock::FullLock };
		TCharArray<4>		   Buffer;
		fileHandle.Open(TXT("./test.txt"), Options)
			.Write("test", 4, { .bEraseSeekPos = true })
			.Read(Buffer.data(), Buffer.Capacity, { .bEraseSeekPos = true })
			.Write("asdf", 4, { .Offset = 4, .Whence = EWhence::Begin, .bEraseSeekPos = true })
			.Read(Buffer.data(), Buffer.Capacity, { .Offset = 4, .Whence = EWhence::Begin, .bEraseSeekPos = true })
			.Close();
		HLVM_LOG(LogTest, info, TXT("Test BoostFileHandle nomap result: {}"), Buffer.c_str());
	}
	{
		FBoostMapFileHandle fileHandle;
		FFileOptions		Options{ .eFileMode = EFileMode::RW, .eFileMapped = EFileMapped::Mapped, .eFileLock = EFileLock::FullLock };
		TCharArray<4>		Buffer;
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

		auto all_matches = FPath::Glob("./", R"(.*Test.*)", true);
		HLVM_LOG(LogTest, info, TXT("Test FPath::Find result:\n{}"), *FPath::DumpJson(all_matches));
	}
}

RECORD(packed_test)
{
#if TEST_STACK_ALLOCATOR
	//  Try use StackMallocator and you will have lifetime object crash on free
	TStackMallocator<> StackMallocator{};
	HLVM_SCOPED_VARIABLE(
		ScopedMallocator, [&]() -> void { SwapMallocator(&StackMallocator); },
		[&]() -> void { SwapMallocator(); });
#else
	FMiMallocator MiMallocator{ { .bNewHeap = true } };
	HLVM_SCOPED_VARIABLE(
		ScopedMallocator, [&]() -> void { SwapMallocator(&MiMallocator); },
		[&]() -> void { SwapMallocator(); });
#endif

	HLVM_LOG(LogTest, info, TXT("Test PackedFileHandle"));

	{
		FPath PackedFileName = "./packed-test";
		FPath PackedTokFile = PackedFileName.ChangeExtension(HLVM_PACKED_TOKEN_FILE_EXT);
		FPath PackedCotFile = PackedFileName.ChangeExtension(HLVM_PACKED_CONTAINER_FILE_EXT);
		FPath PackedJsonlFile = PackedFileName.ChangeExtension(HLVM_JSONL_FILE_EXT);
		HLVM_LOG(LogTest, info, TXT("Test PackedFileHandle write token file: {}"), *PackedTokFile);

		constexpr size_t NumEntries = 8;
		{
#if TEST_FIBER_POOL
			std::vector<boost::fibers::future<void>> jobs;
#else
			std::vector<std::future<void>> jobs;
#endif
			jobs.reserve(4);
			FBoostMapFileHandle fileTokHandle, fileCotHandle, fileJsonlHandle;
			FFileOptions		Options{ .eFileMode = EFileMode::WB, .eFileMapped = EFileMapped::Mapped, .eFileLock = EFileLock::InterProcessLock };
			HLVM_SCOPED_VARIABLE(
				ScopedFileHandle, [&]() -> void {
                    fileTokHandle.Open(PackedTokFile, Options);
                    fileCotHandle.Open(PackedCotFile, Options);
                    fileJsonlHandle.Open(PackedJsonlFile, Options); },
				[&]() -> void {
					fileTokHandle.Close();
					fileCotHandle.Close();
					fileJsonlHandle.Close();
				});

			TVector<std::tuple<FPackedTokenEntryWithPath, FByteVector>> PackedData;
			PackedData.resize(NumEntries);
			size_t _StartPos = 0;
			for (size_t i = 0; i < NumEntries; ++i)
			{
				FPath Path = FPath{ FString::Format(TXT("./test_{}.txt"), i), EPlatformFileType::Packed };
				auto& Entry_Dev = get<0>(PackedData[i]);
				auto& ContentBuffer = get<1>(PackedData[i]);
				// Dummy content, should do some post-processing based on compress and encrypt type in production code
				ContentBuffer.resize(8);

				Entry_Dev.Path = Path.string();

				Entry_Dev.Entry.Data.EncryptType = EEncryptType::No;
				Entry_Dev.Entry.Data.CompressType = ECompressType::No;

				Entry_Dev.Entry.PathHash = Path.GetHash();
				Entry_Dev.Entry.Data.StartPos = _StartPos;
				Entry_Dev.Entry.Data.Size = ContentBuffer.size();
				Entry_Dev.Entry.Data.DecompressSize = ContentBuffer.size();

				_StartPos += S_C(size_t, Entry_Dev.Entry.Data.Size);
			}
			jobs.emplace_back(

#if TEST_FIBER_POOL
				FWorkStealFiberPool::Get()->EnqueueTask(
#else
				FAsync::Launch(EAsyncMode::StandAlone,
#endif
					[&]() {
						for (size_t i = 0; i < PackedData.size(); ++i)
						{
							const auto& Entry_Dev = get<0>(PackedData[i]);
							std::string json = ToJson(Entry_Dev);
							json += "\n";
							fileJsonlHandle.Write(json.c_str(), json.size());
						}
					}));

			FByteVector TokenData;
			FByteVector CotData;
			for (size_t i = 0; i < NumEntries; ++i)
			{
				const auto& Entry_Dev = get<0>(PackedData[i]);
				const auto& ContentBuffer = get<1>(PackedData[i]);
				{
					TBYTE		buffer[FPackedTokenEntry_SerializedSize];
					FByteBuffer buffer_span = buffer;
					bool		bSuccess = SerializeTo(Entry_Dev.Entry, buffer_span);
					HLVM_ENSURE(bSuccess, TXT("SerializeTo failed"));
					std::move(buffer_span.begin(), buffer_span.end(), std::back_inserter(TokenData));
				}
				{
					std::move(ContentBuffer.begin(), ContentBuffer.end(), std::back_inserter(CotData));
				}
			}

			// Compress and Encrypt
			{
				fileCotHandle.Write(CotData.data(), CotData.size());
#if HLVM_PACKED_FILE_WITH_SIGNATURE
				FRSA::SignToFile(TO_CONST_BYTE_BUFFER(CotData), PackedCotFile.AppendExtension(HLVM_RSA_SIGNATURE_EXT));
#endif
			}

			{
				const auto Compressed = FZstd::Compress(TokenData);
#if HLVM_PACKED_TOKEN_FILE_WITH_ENCRYPTION
				const auto Encrypted = FRSA::Encrypt(Compressed);
#else
				const auto& Encrypted = Compressed;
#endif
				fileTokHandle.Write(Encrypted.data(), Encrypted.size());
#if HLVM_PACKED_FILE_WITH_SIGNATURE
				FRSA::SignToFile(TO_CONST_BYTE_BUFFER(Encrypted), PackedTokFile.AppendExtension(HLVM_RSA_SIGNATURE_EXT));
#endif
				/**
				 * Compress, Encrypt and sign must be in the same thread
				 */
				//				jobs.emplace_back([&]() {
				//					auto Compressed = FZstd::Compress(TokenData);
				//					auto Encrypted = FRSA::Encrypt(Compressed);
				//					fileTokHandle.Write(Encrypted.data(), Encrypted.size());
				//					FRSA::SignToFile(TO_CONST_BYTE_BUFFER(Encrypted), PackedTokFile.AppendExtension(HLVM_RSA_SIGNATURE_EXT));
				//				});
			}

			for (auto& job : jobs)
			{
				job.wait();
			}
		}

		{
#if TEST_FIBER_POOL
			FWorkStealFiberPool::Get()->EnqueueTask(
#else
			FAsync::Launch(EAsyncMode::StandAlone,
#endif
										  [&]() {
											  HLVM_LOG(LogTest, info, TXT("Test PackedFileHandle read token file: {}"), *PackedTokFile);
											  FPackedPlatformFile::Get()->Mount(PackedFileName);
											  for (size_t i = 0; i < NumEntries; ++i)
											  {
												  FPackedEntryHandle entryHandle;
												  entryHandle.Open(FString::Format(TXT("./test_{}.txt"), i));
											  }
										  })
				.wait();
		}
	}
}
