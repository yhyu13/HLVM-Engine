/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Platform/PlatformDefinition.h"

#ifdef PLATFORM_WINDOWS
	#include "Platform/GenericPlatformThreadUtil.h"

class FWindowsPlatformThreadUtil final : public FGenericPlatformThreadUtil
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
				HLVM_LOG(LogWindowsPlatform, err, TXT("Invalid thread affinity mode NumThreads {} and {}"),
					Threads.size(), *Config2->ToString());
			}
			if (bNoError)
			{
				for (auto& Thread : Threads)
				{
					auto Mask = DWORD_PTR(0);
					for (const auto& core : Config2->TargetedCores)
					{
						Mask |= DWORD_PTR(1) << core.ID;
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
		return bNoError;
	}
};

FGenericPlatformThreadUtil* FGenericPlatformThreadUtil::sInstance{ new FWindowsPlatformThreadUtil() };

#endif
