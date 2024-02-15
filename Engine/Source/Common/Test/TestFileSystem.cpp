/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Test.h"

#include "Core/FileSystem/BoostFileHandle.h"

DELCARE_LOG_CATEGORY(LogTest)
DEFINE_LOG_CATEGORY(LogTest)

/*
	<test method>
*/
static void test_boostfile_test()
{
	HLVM_LOG(LogTest, info, TXT("Test BoostFileHandle"));
	{
		std::shared_ptr<FBoostFileHandle> fileHandle = std::make_shared<FBoostFileHandle>();
		FFileOptions					  Options{ .eFileMode = EFileMode::RW, .eFileMapped = EFileMapped::NoMapped };
		FFileOpStatus					  Status;
		fileHandle->Open(TXT("./test.txt"), Options, &Status)
			.Write("test", 4, 0, &Status)
			.Close(&Status);
	}
	{
		std::shared_ptr<FBoostFileHandle> fileHandle = std::make_shared<FBoostFileHandle>();
		FFileOptions					  Options{ .eFileMode = EFileMode::R, .eFileMapped = EFileMapped::Mapped };
		FFileOpStatus					  Status;
		char							  Buffer[5] = { 0 };
		fileHandle->Open(TXT("./test.txt"), Options, &Status)
			.Read(Buffer, (sizeof(Buffer) / sizeof(Buffer[0]) - 1), 0, &Status)
			.Close(&Status);
		HLVM_LOG(LogTest, info, TXT("Test BoostFileHandle result: {}"), TO_TCHAR_STR(Buffer));
	}
}
RECORD_TEST_FUNC(boostfile_test)
