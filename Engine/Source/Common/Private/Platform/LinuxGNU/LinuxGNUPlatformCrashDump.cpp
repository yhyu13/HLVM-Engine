/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Platform/PlatformDefinition.h"

#ifdef PLATFORM_LINUXGNU
	#include "Platform/GenericPlatformCrashDump.h"

// Reference : https://stackoverflow.com/a/54427899/6658943
	#include <signal.h> // ::signal, ::raise
	#include <boost/stacktrace.hpp>

namespace hlvm_private
{
	HLVM_STATIC_FUNC void SignalHandler(int)
	{
		::signal(SIGSEGV, SIG_DFL);
		::signal(SIGABRT, SIG_DFL);
		if (!FGenericPlatformCrashDump::CrashDumpFilePath.empty())
		{
			boost::stacktrace::safe_dump_to(FGenericPlatformCrashDump::CrashDumpFilePath);
		}
		::raise(SIGABRT);
	}

	HLVM_NORETURN HLVM_STATIC_FUNC void TerminateHandler()
	{
		if (!FGenericPlatformCrashDump::CrashDumpFilePath.empty())
		{
			boost::stacktrace::safe_dump_to(FGenericPlatformCrashDump::CrashDumpFilePath);
		}
		std::abort();
	}
} // namespace hlvm_private

class FLinuxGNUPlatformCrashDump final : public FGenericPlatformCrashDump
{
protected:
	virtual void InternalInit() final override
	{
		::signal(SIGSEGV, hlvm_private::SignalHandler);
		::signal(SIGABRT, hlvm_private::SignalHandler);

		std::set_terminate(hlvm_private::TerminateHandler);
	}
};

FGenericPlatformCrashDump* FGenericPlatformCrashDump::sInstance{ new FLinuxGNUPlatformCrashDump() };

#endif
