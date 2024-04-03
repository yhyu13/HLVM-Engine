/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Test.h"

#include "Core/Log.h"
#include "Platform/GenericPlatformDebuggerUtil.h"

DECLARE_LOG_CATEGORY(LogTest)

/*
	<test method>
*/
RECORD(test_string, true)
{
	HLVM_LOG(LogTest, trace, TXT("Test performance impact on different order of formatting!"));
	{
		const FStdString& StackTrace = FGenericPlatformDebuggerUtil::GetStackTrace();
		auto			  Message = FString::Format(TXT("Ensure failed: {0}, with '{1}'"), TXT("1!=1"),
						 FString::Format(TXT("1!=1")));

		constexpr int kNumThreads = 1;
		constexpr int kNumIterations = 10;
		constexpr int kNumLoops = 1000;
		double		  time_order_big_first, time_order_small_first;
		{
			auto TestFunc = [&](double& Duration) -> bool {
				FTimer					 Timer;
				std::once_flag			 Flag;
				std::atomic<int>		 Counter{ kNumThreads };
				std::vector<std::thread> threads;
				for (int j = 0; j < kNumThreads; ++j)
				{
					threads.emplace_back([&] {
						std::call_once(Flag, [&Timer] {
							Timer.Reset();
						});
						for (int k = 0; k < kNumLoops; ++k)
						{
							const FString& msg = FString::Format(TXT("{1} at {2}:{3}\n{0}"), *StackTrace, Message,
								TXT(__FILE__), __LINE__);
						}
						if (--Counter == 0)
						{
							Duration = Timer.Mark();
						}
					});
				}

				for (std::thread& t : threads)
				{
					t.join();
				}
				HLVM_LOG(LogTest, info, TXT("test took {0:f}"), Duration);
				return true;
			};

			time_order_big_first = RunTestAndCalculateAvg(TestFunc, kNumIterations);
			HLVM_LOG(LogTest, info, TXT("order_big_first avg took {0:f}"), time_order_big_first);
		}

		{
			auto TestFunc = [&](double& Duration) -> bool {
				FTimer					 Timer;
				std::once_flag			 Flag;
				std::atomic<int>		 Counter{ kNumThreads };
				std::vector<std::thread> threads;
				for (int j = 0; j < kNumThreads; ++j)
				{
					threads.emplace_back([&] {
						std::call_once(Flag, [&Timer] {
							Timer.Reset();
						});
						for (int k = 0; k < kNumLoops; ++k)
						{
							const FString& msg = FString::Format(TXT("{0} at {1}:{2}\n{3}"), Message,
								TXT(__FILE__), __LINE__, *StackTrace);
						}
						if (--Counter == 0)
						{
							Duration = Timer.Mark();
						}
					});
				}

				for (std::thread& t : threads)
				{
					t.join();
				}
				HLVM_LOG(LogTest, info, TXT("test took {0:f}"), Duration);
				return true;
			};
			time_order_small_first = RunTestAndCalculateAvg(TestFunc, kNumIterations);
			HLVM_LOG(LogTest, info, TXT("order_small_first avg took {0:f}"), time_order_small_first);
		}

		if (time_order_big_first < time_order_small_first)
		{
			if ((time_order_small_first - time_order_big_first) / time_order_small_first <= 0.05)
			{
				HLVM_LOG(LogTest, info, TXT("difference smaller than 5%"));
			}
			else
			{
				HLVM_LOG(LogTest, info, TXT("order_big_first is faster than order_small_first"));
			}
		}
		else
		{
			if ((time_order_big_first - time_order_small_first) / time_order_big_first <= 0.05)
			{
				HLVM_LOG(LogTest, info, TXT("difference smaller than 5%"));
			}
			else
			{
				HLVM_LOG(LogTest, info, TXT("order_small_first is faster than order_big_first"));
			}
		}
	}
}

#include "Core/Name.h"

RECORD(test_name, true)
{
	HLVM_LOG(LogTest, trace, TXT("Test FName!"));
	FName name("test");
	{
		HLVM_LOG(LogTest, info, TXT("name: {0}, ref {1}"), *name.ToString(), name.NumRef());
		for (int i = 0; i < 1000000; ++i)
		{
			FName name2 = name;
			if (i % 100000 == 0)
			{
				HLVM_LOG(LogTest, info, TXT("name: {0}, ref {1}"), *name.ToString(), name.NumRef());
			}
		}
		HLVM_LOG(LogTest, info, TXT("name: {0}, ref {1}"), *name.ToString(), name.NumRef());
	}
}
