/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Platform/PlatformDefinition.h"

#ifdef PLATFORM_LINUXGNU
	#include "Platform/GenericPlatformCrashDump.h"
	#include "Platform/GenericPlatformStackTrace.h"

// Reference :
// https://stackoverflow.com/questions/77005/how-to-automatically-generate-a-stacktrace-when-my-program-crashes
// https://stackoverflow.com/a/54427899/6658943
	#include <signal.h> // ::signal, ::raise
	#include <boost/stacktrace.hpp>

namespace hlvm_private
{
	HLVM_STATIC_FUNC void SignalHandler(int)
	{
		::signal(SIGSEGV, SIG_DFL);
		::signal(SIGABRT, SIG_DFL);
		::signal(SIGINT, SIG_DFL);
		::signal(SIGTERM, SIG_DFL);
		if (!FGenericPlatformCrashDump::CrashDumpFilePath.empty())
		{
			boost::stacktrace::safe_dump_to(FGenericPlatformCrashDump::CrashDumpFilePath);
		}
		if (!FGenericPlatformCrashDump::CrashStackFilePath.empty())
		{
			// open a file stream and dump to it
			std::ofstream file(FGenericPlatformCrashDump::CrashStackFilePath);
			file << (FGenericPlatformStackTrace::GetStackTraceStream()).view() << std::endl;
			file.close();
		}

		::raise(SIGABRT);
	}

	HLVM_NORETURN HLVM_STATIC_FUNC void TerminateHandler()
	{
		if (!FGenericPlatformCrashDump::CrashDumpFilePath.empty())
		{
			boost::stacktrace::safe_dump_to(FGenericPlatformCrashDump::CrashDumpFilePath);
		}
		if (!FGenericPlatformCrashDump::CrashStackFilePath.empty())
		{
			// open a file stream and dump to it
			std::ofstream file(FGenericPlatformCrashDump::CrashStackFilePath);
			file << (FGenericPlatformStackTrace::GetStackTraceStream()).view() << std::endl;
			file.close();
		}
		std::abort();
	}
} // namespace hlvm_private

class FLinuxGNUPlatformCrashDump final : public FGenericPlatformCrashDump
{
protected:
	virtual void InternalInit() final override
	{
		// Register signal handler
		::signal(SIGSEGV, hlvm_private::SignalHandler);
		::signal(SIGABRT, hlvm_private::SignalHandler);
		::signal(SIGINT, hlvm_private::SignalHandler);
		::signal(SIGTERM, hlvm_private::SignalHandler);
		// Register terminate handler
		std::set_terminate(hlvm_private::TerminateHandler);
	}
};

FGenericPlatformCrashDump* FGenericPlatformCrashDump::sInstance{ new FLinuxGNUPlatformCrashDump() };

#endif
