/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Platform/GenericPlatformCrashDump.h"

#include <boost/stacktrace.hpp>
#include <boost/filesystem.hpp>

FPath FGenericPlatformCrashDump::CrashDumpFilePath{ "./hlvm_crash.dump" };

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
			std::ifstream file(CrashDumpFilePath);

			auto			   st = boost::stacktrace::stacktrace::from_dump(file);
			std::ostringstream backtraceStream;
			backtraceStream << st << std::endl;

			// TODO Implement cerr log device (belong to warn and above verbosity)
			std::cerr << CrashDumpFilePath.ToCharStr() << "\n";
			std::cerr << backtraceStream.str();

			// sending the code from st

			file.close();
		}
		if (DeleteDumpAfterReport)
		{
			boost::filesystem::remove(CrashDumpFilePath);
		}
	}
}
