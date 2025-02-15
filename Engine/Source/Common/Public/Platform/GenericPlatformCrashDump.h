/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "GenericPlatform.h"
#include "Platform/FileSystem/Path.h"

class FGenericPlatformCrashDump
{
public:
	NOCOPYMOVE(FGenericPlatformCrashDump)
	FGenericPlatformCrashDump() = default;
	virtual ~FGenericPlatformCrashDump() = default;

	/**
	 * Initialize the crash dump handler, shall register to crash signal in each platform
	 */
	HLVM_STATIC_FUNC void Init();

	/**
	 * @brief Terminate the program intentionally by user
	 * @details will call std::terminate which contain platform terminate handler if possible, otherwise call std::abort
	 * @param DumpStackStrace - Whether to dump the stack trace before terminate
	 */
	HLVM_NORETURN HLVM_NOINLINE_FUNC HLVM_STATIC_FUNC void Terminate(bool DumpStackStrace = true);

	/**
	 * @brief Display previous time crash report if exists
	 * @param DeleteDumpAfterReport - Whether to delete the dump file after reporting
	 */
	HLVM_STATIC_FUNC void ReportDump(bool DeleteDumpAfterReport = true);

protected:
	virtual void InternalInit() = 0;

public:
	HLVM_STATIC_VAR FPath CrashDumpFilePath;

protected:
	HLVM_STATIC_VAR FGenericPlatformCrashDump* sInstance;
};
