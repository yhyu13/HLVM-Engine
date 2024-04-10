/**
 * Copyright (c) 2024. MIT License. All rights reserved.
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
	HLVM_STATIC_FUNC void Init()
	{
		static std::once_flag once;
		std::call_once(once, []() {
			{
				sInstance->InternalInit();
			}
		});
	}

	/**
	 * Terminate the program
	 * @param DumpStackStrace - Whether to dump the stack trace before terminate
	 */
	HLVM_NORETURN HLVM_NOINLINE_FUNC HLVM_STATIC_FUNC void Terminate(bool DumpStackStrace = true);

	/**
	 * Display (optionally Send) the last time crash report
	 */
	HLVM_STATIC_FUNC void ReportDump(bool DeleteDumpAfterReport = true);

public:
	HLVM_STATIC_VAR FPath CrashDumpFilePath;

protected:
	virtual void InternalInit() = 0;

	HLVM_STATIC_VAR FGenericPlatformCrashDump* sInstance;
};
