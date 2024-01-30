/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Test.h"

#include "Core/Parallel/Lock.h"
#include "Core/Log.h"
#include "Ultility/Timer.h"

#include <thread>
#include <vector>

DELCARE_LOG_CATEGORY(LogTest)
DEFINE_LOG_CATEGORY(LogTest)

/*
	<test method>
*/
RECORD(lock_test,
	{
		auto LogDevice = std::make_shared<FSpdlogConsoleDevice>();
		FLogRedirector::Get()->AddDevice(LogDevice);

		double time_no_lock, time_lock;
		{
			HLVM_LOG(LogTest, info, TXT("No lock : Create 10 threads and adds to i"));
			int i = 0;
			// Create 10 threads and adds to i
			std::vector<std::thread> threads;
			FTimer					 timer{ true };
			for (int j = 0; j < 10; ++j)
			{
				threads.emplace_back([&i] {
					for (int k = 0; k < 1000; ++k)
					{
						i++;
					}
				});
			}

			for (std::thread& t : threads)
			{
				t.join();
			}
			time_no_lock = timer.Mark();
			HLVM_LOG(LogTest, info, TXT("i = {0:d}, took {1:f}"), i, time_no_lock);
		}

		{
			HLVM_LOG(LogTest, info, TXT("With lock : Create 10 threads and adds to i"));
			int i = 0;
			// Create 10 threads and adds to i
			std::vector<std::thread> threads;
			FAtomicFlag				 lock;
			FTimer					 timer{ true };
			for (int j = 0; j < 10; ++j)
			{
				threads.emplace_back([&i, &lock] {
					for (int k = 0; k < 1000; ++k)
					{
						FAtomicLockGuard lock_guard{ lock };
						i++;
					}
				});
			}

			for (std::thread& t : threads)
			{
				t.join();
			}
			time_lock = timer.Mark();
			HLVM_LOG(LogTest, info, TXT("i = {0:d}, took {1:f}"), i, time_lock);
		}
		HLVM_LOG(LogTest, info, TXT("No lock = {0:.2f}x With lock"), time_lock / time_no_lock);
	})