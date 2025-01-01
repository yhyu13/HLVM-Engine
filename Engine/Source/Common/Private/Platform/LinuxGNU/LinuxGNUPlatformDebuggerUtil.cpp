/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Platform/PlatformDefinition.h"

#ifdef PLATFORM_LINUXGNU
	#include "Platform/GenericPlatformDebuggerUtil.h"
	#include "Utility/Timer.h"
	#include "Core/Parallel/Lock.h"

// https://opensource.com/article/18/1/how-debuggers-work
// https://forum.juce.com/t/detecting-if-a-process-is-being-run-under-a-debugger/2098
	#include <sys/ptrace.h>
	#include <sys/wait.h>
	#include <sys/mman.h>
	#include <fcntl.h>
	#include <unistd.h>

class FLinuxGNUPlatformDebuggerUtil final : public FGenericPlatformDebuggerUtil
{
protected:
	virtual bool InternalIsDebuggerPresent() final override
	{
		using namespace std::chrono_literals;

		static FTimer	   PeriodicTimer{ 1s, true };
		static int		   underDebugger = 0;
		static bool		   bInit = false;
		static FAtomicFlag Lock;
		ATOMIC_LOCK_GUARD(Lock);
		if (!bInit || PeriodicTimer.Check(false))
		{
			underDebugger = 0;

			if (!underDebugger)
			{
				/**
				 * LD_PRELOAD is an environment variable in Linux that allows you to preload shared libraries before running a program.
				 * This is useful for intercepting and modifying the behavior of functions in a program by using a custom shared library.
				 */
				char* ld_preload = getenv("LD_PRELOAD");
				if (ld_preload != nullptr)
				{
					HLVM_LOG(LogLinuxGNUPlatform, debug, TXT("Debugger detected: LD_PRELOAD failed"));
					underDebugger = 1;
				}
			}

			if (!underDebugger)
			{
				/**
				 * TracerPid is a process ID that indicates the process that is tracing the current process.
				 */
				FILE* fp = fopen("/proc/self/status", "r");
				if (fp != nullptr)
				{
					char buf[1024];
					while (fgets(buf, sizeof(buf), fp))
					{
						if (strncmp(buf, "TracerPid:", 10) == 0)
						{
							int tracer_pid = atoi(buf + 10);
							if (tracer_pid != 0)
							{
								HLVM_LOG(LogLinuxGNUPlatform, debug, TXT("Debugger detected: TracerPid failed"));
								underDebugger = 1;
								break;
							}
						}
					}
					fclose(fp);
				}
			}

			if (!underDebugger)
			{
				/**
				 * Ptrace is a system call that allows a process to trace another process.
				 */
				if (ptrace(PTRACE_TRACEME, 0, 1, 0) < 0)
				{
					HLVM_LOG(LogLinuxGNUPlatform, debug, TXT("Debugger detected: Ptrace failed"));
					underDebugger = 1;
				}
			}

			if (!underDebugger)
			{
				/**
				 * Check for Debugger Breakpoints
				 */
				struct sigaction sa;
				sigemptyset(&sa.sa_mask);
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
				sa.sa_handler = SIG_DFL;
	#pragma clang diagnostic pop
				sa.sa_flags = 0;
				if (sigaction(SIGTRAP, &sa, nullptr) < 0)
				{
					HLVM_LOG(LogLinuxGNUPlatform, debug, TXT("Debugger detected: breakpoint failed"));
					underDebugger = 1;
				}
			}

			if (!underDebugger)
			{
				/**
				 * Check for Debugger Memory Access
				 */
				void* buf = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
				memset(buf, 0, 4096);
				if (mprotect(buf, 4096, PROT_READ) < 0)
				{
					HLVM_LOG(LogLinuxGNUPlatform, debug, TXT("Debugger detected: mmap failed"));
					underDebugger = 1;
				}
				munmap(buf, 4096);
			}

			if (!underDebugger)
			{
				int fd = open("/dev/null", O_RDWR);
				if (fcntl(fd, F_GETFL) < -1)
				{
					HLVM_LOG(LogLinuxGNUPlatform, debug, TXT("Debugger detected: access failed"));
					underDebugger = 1;
				}
				close(fd);
			}

			PeriodicTimer.Reset();
			bInit = true;
		}
		return underDebugger == 1;
	}
};

FGenericPlatformDebuggerUtil* FGenericPlatformDebuggerUtil::sInstance{ new FLinuxGNUPlatformDebuggerUtil() };

#endif
