/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Platform/PlatformDefinition.h"

#ifdef PLATFORM_LINUXGNU
	#include "Platform/GenericPlatformThreadUtil.h"

class FLinuxGNUPlatformThreadUtil final : public FGenericPlatformThreadUtil
{
protected:
	virtual bool InternalSetThreadsWithAffinity(const TVector<boost::thread*>& Threads, const FThreadAffinityMode& AffinityMode) final override
	{
		bool bNoError = true;
		// TODO : support other thread affinity mode
		{
			auto Config2 = S_C(const FThreadAffinityMode2*, AffinityMode);
			bNoError = Threads.size() == Config2->NumThreads && Config2->IsValid();
			if (!bNoError)
			{
				HLVM_LOG(LogLinuxGNUPlatform, err, TXT("Invalid thread affinity mode NumThreads {} and {}"),
					Threads.size(), *Config2->ToString());
			}
			if (bNoError)
			{
				for (auto& Thread : Threads)
				{
					// Create a cpu_set_t object representing a set of CPUs. Clear it and mark
					cpu_set_t cpuset;
					CPU_ZERO(&cpuset);
					for (const auto& core : Config2->TargetedCores)
					{
						CPU_SET(core.ID, &cpuset);
					}
					int rc = pthread_setaffinity_np(Thread->native_handle(),
						sizeof(cpu_set_t), &cpuset);
					if (rc != 0)
					{
						HLVM_LOG(LogLinuxGNUPlatform, err, TXT("Error calling pthread_setaffinity_np with erno {}"),
							rc);
						bNoError = false;
					}
				}
			}
		}
		return bNoError;
	}
};

FGenericPlatformThreadUtil* FGenericPlatformThreadUtil::sInstance{ new FLinuxGNUPlatformThreadUtil() };

#endif
