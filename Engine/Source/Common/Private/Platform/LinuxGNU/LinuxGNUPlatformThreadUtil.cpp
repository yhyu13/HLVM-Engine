/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "Platform/PlatformDefinition.h"

#ifdef PLATFORM_LINUXGNU
	#include "Platform/GenericPlatformThreadUtil.h"

namespace hlvm_private
{
	HLVM_STATIC_FUNC bool SetBoostThreadPriority(boost::thread* Thread, const EThreadPriority& Priority)
	{
		bool bNoError = true;
		// Reference : https://access.redhat.com/documentation/zh-cn/red_hat_enterprise_linux_for_real_time/7/html/reference_guide/chap-priorities_and_policies
		int policy = SCHED_OTHER;
		switch (Priority)
		{
			case EThreadPriority::Normal:
				break;
			case EThreadPriority::High:
				policy = SCHED_FIFO; // SCHED_FIFO is perferred over SCHED_RR
				break;
			case EThreadPriority::Low:
				policy = SCHED_IDLE;
				break;
			case EThreadPriority::_NUM:
			default:
				HLVM_ASSERT_F(false, TXT("Unknown thread priority"));
				break;
		}
		struct sched_param param;
		param.sched_priority = 0;
		int rc = pthread_setschedparam(Thread->native_handle(), policy, &param);
		if (rc != 0)
		{
			HLVM_LOG(LogLinuxGNUPlatform, err, TXT("Error calling pthread_setschedparam with erno {}"),
				rc);
			bNoError = false;
		}
		return bNoError;
	}
} // namespace hlvm_private

class FLinuxGNUPlatformThreadUtil final : public FGenericPlatformThreadUtil
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
						// Create a cpu_set_t object representing a set of CPUs. Clear it and mark
						cpu_set_t cpuset;
						CPU_ZERO(&cpuset);
						const auto& core = Config1->TargetedCores[i];
						{
							if (core.Type == ECoreType::Physical)
							{
								CPU_SET(core.ID * HLVM_PLATFORM_SIMT_MULTIPLIER, &cpuset);
							}
							else if (core.Type == ECoreType::Logical)
							{
								CPU_SET(core.ID, &cpuset);
							}
							else
							{
								HLVM_ASSERT_F(false, TXT("Unknown core type"));
							}
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
						// Create a cpu_set_t object representing a set of CPUs. Clear it and mark
						cpu_set_t cpuset;
						CPU_ZERO(&cpuset);
						for (const auto& core : Config2->TargetedCores)
						{
							if (core.Type == ECoreType::Physical)
							{
								CPU_SET(core.ID * HLVM_PLATFORM_SIMT_MULTIPLIER, &cpuset);
							}
							else if (core.Type == ECoreType::Logical)
							{
								CPU_SET(core.ID, &cpuset);
							}
							else
							{
								HLVM_ASSERT_F(false, TXT("Unknown core type"));
							}
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
						// Create a cpu_set_t object representing a set of CPUs. Clear it and mark
						cpu_set_t cpuset;
						CPU_ZERO(&cpuset);
						for (const auto& [threads, cores] : Config3->TargetedCores)
						{
							if (std::find(threads.begin(), threads.end(), i) != threads.end())
							{
								for (const auto& core : cores)
								{
									if (core.Type == ECoreType::Physical)
									{
										CPU_SET(core.ID * HLVM_PLATFORM_SIMT_MULTIPLIER, &cpuset);
									}
									else if (core.Type == ECoreType::Logical)
									{
										CPU_SET(core.ID, &cpuset);
									}
									else
									{
										HLVM_ASSERT_F(false, TXT("Unknown core type"));
									}
								}
							}
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
		}
		else
		{
			HLVM_ASSERT_F(false, TXT("Unknown thread affinity mode"));
			bNoError = false;
		}
		return bNoError;
	}
};

FGenericPlatformThreadUtil* FGenericPlatformThreadUtil::sInstance{ new FLinuxGNUPlatformThreadUtil() };

#endif
