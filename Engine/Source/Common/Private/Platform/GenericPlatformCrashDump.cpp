/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Platform/GenericPlatformCrashDump.h"

#include <boost/stacktrace.hpp>
#include <boost/filesystem.hpp>

FPath FGenericPlatformCrashDump::CrashDumpFilePath{ "./hlvm_crash.dump" };

void FGenericPlatformCrashDump::Init()
{
	static std::once_flag once;
	std::call_once(once, []() {
		{
			sInstance->InternalInit();
			sInstance->ReportDump();
		}
	});
}

HLVM_NORETURN void FGenericPlatformCrashDump::Terminate(bool DumpStackStrace)
{
	if (std::get_terminate())
	{
		if (!DumpStackStrace)
		{
			CrashDumpFilePath.clear();
		}
		std::terminate();
	}
	else
	{
		if (DumpStackStrace && !CrashDumpFilePath.empty())
		{
			boost::stacktrace::safe_dump_to(CrashDumpFilePath);
		}
		std::abort();
	}
}

void FGenericPlatformCrashDump::ReportDump(bool DeleteDumpAfterReport)
{
	if (boost::filesystem::exists(CrashDumpFilePath))
	{
		{
			// https://www.boost.org/doc/libs/1_65_0/doc/html/stacktrace/getting_started.html
			// Quote: "Writing a signal handler requires high attention! Only a few system calls allowed in signal handlers,
			// so there's no cross platform way to print a stacktrace without a risk of deadlocking.
			// The only way to deal with the problem - dump raw stacktrace into file/socket and parse it on program restart."
			std::ifstream file(CrashDumpFilePath);
			auto		  st = boost::stacktrace::stacktrace::from_dump(file);
			auto		  human_readable = boost::stacktrace::to_string(st);
			std::cerr << CrashDumpFilePath.ToCharCStr() << "\n";
			std::cerr << human_readable.c_str();
			HLVM_LOG(LogCrashDump, warn, TXT("Previous crash dump: {}\n{}"),
				*CrashDumpFilePath, TO_TCHAR_CSTR(human_readable.c_str()));
			file.close();
		}
		if (DeleteDumpAfterReport)
		{
			boost::filesystem::remove(CrashDumpFilePath);
		}
	}
}
