/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "Platform/GenericPlatformCrashDump.h"

#include <boost/stacktrace.hpp>
#include <boost/filesystem.hpp>

FPath FGenericPlatformCrashDump::CrashDumpFilePath{ "./hlvm_crash.dump" };
FPath FGenericPlatformCrashDump::CrashStackFilePath{ "./hlvm_crash.stack" };

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
			CrashStackFilePath.clear();
		}
		std::terminate();
	}
	else
	{
		if (DumpStackStrace && !CrashDumpFilePath.empty())
		{
			boost::stacktrace::safe_dump_to(CrashDumpFilePath);
		}
		if (DumpStackStrace && !CrashStackFilePath.empty())
		{
			// open a file stream and dump to it
			std::ofstream file(CrashStackFilePath);
			file << (FGenericPlatformStackTrace::GetStackTraceStream()).view() << std::endl;
			file.close();
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
			std::cerr << "Previous crash dump: ";
			std::cerr << CrashDumpFilePath.ToCharCStr() << "\n";
			std::cerr << human_readable.c_str();
			// logging system may not be avaiable at report previous crash dump
			HLVM_LOG(LogCrashDump, warn, TXT("Previous crash dump: {}\n{}"),
				*CrashDumpFilePath, TO_TCHAR_CSTR(human_readable.c_str()));
			file.close();
		}
		if (DeleteDumpAfterReport)
		{
			boost::filesystem::remove(CrashDumpFilePath);
		}
	}

	if (boost::filesystem::exists(CrashStackFilePath))
	{
		{
			std::ifstream file(CrashStackFilePath);
			// read as string and log to cerr and hlvm log
			std::stringstream buffer;
			buffer << file.rdbuf();
			std::cerr << "Previous crash stack: ";
			std::cerr << CrashStackFilePath.ToCharCStr() << "\n";
			std::cerr << buffer.str();
			HLVM_LOG(LogCrashDump, warn, TXT("Previous crash stack: {}\n{}"),
				*CrashStackFilePath, TO_TCHAR_CSTR(buffer.str().c_str()));
		}
		if (DeleteDumpAfterReport)
		{
			boost::filesystem::remove(CrashStackFilePath);
		}
	}
}
