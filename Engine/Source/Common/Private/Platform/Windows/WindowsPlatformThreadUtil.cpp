/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Platform/PlatformDefinition.h"

#ifdef PLATFORM_WINDOWS
	#include "Platform/GenericPlatformThreadUtil.h"

namespace hlvm_private
{
	HLVM_STATIC_FUNC bool SetBoostThreadPriority(boost::thread* Thread, const EThreadPriority& Priority)
	{
		bool bNoError = true;
		// Reference : https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadpriority
		int policy = THREAD_PRIORITY_NORMAL;
		switch (Priority)
		{
			case EThreadPriority::Normal:
				break;
			case EThreadPriority::High:
				policy = THREAD_PRIORITY_ABOVE_NORMAL;
				break;
			case EThreadPriority::Low:
				policy = THREAD_PRIORITY_BELOW_NORMAL;
				break;
			case EThreadPriority::HLVM_NUM:
				HLVM_ASSERT(false, TXT("Unknown thread priority"));
				break;
		}
		DWORD dwError, dwThreadPri;
		if (!SetThreadPriority(GetCurrentThread(), policy))
		{
			DWORD dwErr = GetLastError();
			HLVM_LOG(LogWindowsPlatform, err, TXT("Error calling SetThreadPriority with GLE {}"),
				dwErr);
			bNoError = false;
		}
		return bNoError;
	}
} // namespace hlvm_private

class FWindowsPlatformThreadUtil final : public FGenericPlatformThreadUtil
{
protected:
	virtual bool InternalSetThreadsWithAffinity(const TVector<boost::thread*>& Threads, const FThreadAffinityMode& AffinityMode) final override
	{
		bool bNoError = true;
		if (auto Config1 = S_C(const FThreadAffinityMode1*, AffinityMode))
		{
			bNoError = (Threads.size() == Config1->NumThreads && Config1->IsValid());
			if (!bNoError)
			{
				HLVM_LOG(LogLinuxGNUPlatform, err, TXT("Invalid thread affinity mode NumThreads {} and {}"),
					Threads.size(), *Config1->ToString());
			}
			if (bNoError)
			{
				for (size_t i = 0; i < Threads.size(); ++i)
				{
					auto Thread = Threads[i];
					// Set thread prioirty
					bNoError = hlvm_private::SetBoostThreadPriority(Thread, Config1->Priority);

					// Set cpu affinity
					{
						auto		Mask = DWORD_PTR(0);
						const auto& core = Config1->TargetedCores[i];
						{
							if (core.Type == ECoreType::Physical)
							{
								Mask |= DWORD_PTR(1) << (core.ID * HLVM_PLATFORM_SIMT_MULTIPLIER);
							}
							else if (core.Type == ECoreType::Logical)
							{
								Mask |= DWORD_PTR(1) << core.ID;
							}
							else
							{
								HLVM_ASSERT(false, TXT("Unknown core type"));
							}
						}
						DWORD_PTR dw = SetThreadAffinityMask(Thread->native_handle(), Mask);
						if (dw == 0)
						{
							DWORD dwErr = GetLastError();
							HLVM_LOG(LogWindowsPlatform, err, TXT("Error calling SetThreadAffinityMask with GLE {}"),
								dwErr);
							bNoError = false;
						}
					}
				}
			}
		}
		else if (auto Config2 = S_C(const FThreadAffinityMode2*, AffinityMode))
		{
			bNoError = (Threads.size() == Config2->NumThreads && Config2->IsValid());
			if (!bNoError)
			{
				HLVM_LOG(LogLinuxGNUPlatform, err, TXT("Invalid thread affinity mode NumThreads {} and {}"),
					Threads.size(), *Config2->ToString());
			}
			if (bNoError)
			{
				for (auto& Thread : Threads)
				{
					// Set thread prioirty
					bNoError = hlvm_private::SetBoostThreadPriority(Thread, Config2->Priority);

					// Set cpu affinity
					{
						auto Mask = DWORD_PTR(0);
						for (const auto& core : Config2->TargetedCores)
						{
							if (core.Type == ECoreType::Physical)
							{
								Mask |= DWORD_PTR(1) << (core.ID * HLVM_PLATFORM_SIMT_MULTIPLIER);
							}
							else if (core.Type == ECoreType::Logical)
							{
								Mask |= DWORD_PTR(1) << core.ID;
							}
							else
							{
								HLVM_ASSERT(false, TXT("Unknown core type"));
							}
						}
						DWORD_PTR dw = SetThreadAffinityMask(Thread->native_handle(), Mask);
						if (dw == 0)
						{
							DWORD dwErr = GetLastError();
							HLVM_LOG(LogWindowsPlatform, err, TXT("Error calling SetThreadAffinityMask with GLE {}"),
								dwErr);
							bNoError = false;
						}
					}
				}
			}
		}
		else if (auto Config3 = S_C(const FThreadAffinityMode3*, AffinityMode))
		{
			bNoError = (Threads.size() == Config3->NumThreads && Config3->IsValid());
			if (!bNoError)
			{
				HLVM_LOG(LogLinuxGNUPlatform, err, TXT("Invalid thread affinity mode NumThreads {} and {}"),
					Threads.size(), *Config3->ToString());
			}
			if (bNoError)
			{
				for (size_t i = 0; i < Threads.size(); ++i)
				{
					auto Thread = Threads[i];
					// Set thread prioirty
					bNoError = hlvm_private::SetBoostThreadPriority(Thread, Config3->Priority);

					// Set cpu affinity
					{
						auto Mask = DWORD_PTR(0);
						for (const auto& [threads, cores] : Config3->TargetedCores)
						{
							if (std::find(threads.begin(), threads.end(), i) != threads.end())
							{
								for (const auto& core : cores)
								{
									if (core.Type == ECoreType::Physical)
									{
										Mask |= DWORD_PTR(1) << (core.ID * HLVM_PLATFORM_SIMT_MULTIPLIER);
									}
									else if (core.Type == ECoreType::Logical)
									{
										Mask |= DWORD_PTR(1) << core.ID;
									}
									else
									{
										HLVM_ASSERT(false, TXT("Unknown core type"));
									}
								}
							}
						}
						DWORD_PTR dw = SetThreadAffinityMask(Thread->native_handle(), Mask);
						if (dw == 0)
						{
							DWORD dwErr = GetLastError();
							HLVM_LOG(LogWindowsPlatform, err, TXT("Error calling SetThreadAffinityMask with GLE {}"),
								dwErr);
							bNoError = false;
						}
					}
				}
			}
		}
		else
		{
			HLVM_ASSERT(false, TXT("Unknown thread affinity mode"));
			bNoError = false;
		}
		return bNoError;
	}
};

FGenericPlatformThreadUtil* FGenericPlatformThreadUtil::sInstance{ new FWindowsPlatformThreadUtil() };

#endif
