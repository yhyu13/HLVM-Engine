/**
 * Copyright (c) 2024. MIT License. All rights reserved.
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
	boost::system::error_code ec;
	if (boost::filesystem::exists(CrashDumpFilePath, ec))
	{
		std::ifstream file(CrashDumpFilePath);

		auto			   st = boost::stacktrace::stacktrace::from_dump(file);
		std::ostringstream backtraceStream;
		backtraceStream << st << std::endl;

		// TODO Implement cerr log device (belong to warn and above verbosity)
		std::cerr << backtraceStream.str();

		// sending the code from st

		file.close();
		if (DeleteDumpAfterReport)
		{
			boost::filesystem::remove(CrashDumpFilePath);
		}
	}
	else if (ec)
	{
		// TODO Implement cout log device (belong to info and below verbosity)
		std::cout << "Error: " << ec.message() << std::endl;
	}
}
