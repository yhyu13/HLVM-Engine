/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Test.h"

#include "Core/Parallel/Lock.h"
#include "Core/Parallel/ConcurrentQueue.h"
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
						ATOMIC_LOCK_GUARD(lock);
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
		double ratio = time_lock / time_no_lock;
		double efficient = 10 / ratio * 100;
		HLVM_LOG(LogTest, info, TXT("No lock = {0:.2f}x With lock, thus lock is {1:.2f}% efficient"), ratio, efficient);
	})

RECORD(queue_test,
	{
		HLVM_LOG(LogTest, info, TXT("Queue test:"));
		double time_concurrent, time_lock;
		{
			HLVM_LOG(LogTest, info, TXT("Queue test #1 Concurrent"));
			auto Queue = FConcurrentQueue<int>();

			// Multi thread pushing and poping
			std::vector<std::thread> threads;
			FTimer					 timer{ true };
			for (int i = 0; i < 10; ++i)
			{
				threads.emplace_back([&Queue] {
					for (int j = 0; j < 1000; ++j)
					{
						Queue.Push(j);
					}
				});

				threads.emplace_back([&Queue] {
					for (int j = 0; j < 1000;)
					{
						int val;
						if (Queue.PopFront(val))
						{
							++j;
						}
					}
				});
			}

			for (std::thread& t : threads)
			{
				t.join();
			}

			time_concurrent = timer.Mark();
			HLVM_LOG(LogTest, info, TXT("Queue test #1 took {0:f}, queue size {1:d}"), time_concurrent, Queue.Num());
		}

		{
			HLVM_LOG(LogTest, info, TXT("Queue test #2 std::queue"));
			auto Queue = std::queue<int>();

			// Multi thread pushing and poping
			std::vector<std::thread> threads;
			FAtomicFlag				 lock;
			FTimer					 timer{ true };
			for (int i = 0; i < 10; ++i)
			{
				threads.emplace_back([&Queue, &lock] {
					for (int j = 0; j < 1000; ++j)
					{
						ATOMIC_LOCK_GUARD(lock);
						Queue.push(j);
					}
				});

				threads.emplace_back([&Queue, &lock] {
					for (int j = 0; j < 1000; ++j)
					{
						ATOMIC_LOCK_GUARD(lock);
						int val;
						val = Queue.front();
						Queue.pop();
					}
				});
			}

			for (std::thread& t : threads)
			{
				t.join();
			}

			time_lock = timer.Mark();
			HLVM_LOG(LogTest, info, TXT("Queue test #2 took {0:f}, queue size {1:d}"), time_lock, Queue.size());
		}

		double ratio = time_lock / time_concurrent;
		double per_thread_gain = ratio / 10 * 100;
		HLVM_LOG(LogTest, info, TXT("Queue test #1 = {0:.2f}x Queue test #2, per thread gain {1:.2f}%"), ratio, per_thread_gain);
	})