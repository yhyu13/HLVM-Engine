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
#include <boost/lockfree/queue.hpp>

DELCARE_LOG_CATEGORY(LogTest)
DEFINE_LOG_CATEGORY(LogTest)

using TestFuncType = std::function<bool(double&)>;

// TODO : implment smoothed averge time mesaruement
// i.e. run test case mutiple times with timer and calculate averge by removing max and min
double RunTestAndCalculateAvg(const TestFuncType& func, int num_iterations)
{
	std::vector<double> times;
	for (int i = 0; i < num_iterations; ++i)
	{
		double duration;
		func(duration);
		times.emplace_back(duration);
	}
	{
		auto mm = std::minmax_element(begin(times), end(times));
		std::iter_swap(mm.first, end(times) - 2);
		std::iter_swap(mm.second, end(times) - 1);
	}
	double avg = 0.0;
	for (int i = 0; i < num_iterations - 2; ++i)
	{
		avg += times[i];
	}
	return avg / (num_iterations - 2);
}

/*
	<test method>
*/
RECORD(lock_test,
	{
		auto LogDevice = std::make_shared<FSpdlogConsoleDevice>();
		FLogRedirector::Get()->AddDevice(LogDevice);

		constexpr int kNumThreads = 10;
		constexpr int kNumIterations = 10;
		double		  time_no_lock, time_lock;
		{
			HLVM_LOG(LogTest, info, TXT("No lock : Create 10 threads and adds to i"));
			auto TestFunc = [&](double& Duration) -> bool {
				int						 i = 0;
				FTimer					 Timer;
				std::once_flag			 Flag;
				std::atomic<int>		 Counter{ kNumThreads };
				std::vector<std::thread> threads;
				FTimer					 timer{ true };
				for (int j = 0; j < kNumThreads; ++j)
				{
					threads.emplace_back([&i, &Timer, &Flag, &Counter, &Duration] {
						std::call_once(Flag, [&Timer] {
							Timer.Reset();
						});
						for (int k = 0; k < 1000; ++k)
						{
							i++;
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

				HLVM_LOG(LogTest, info, TXT("i = {0:d}, took {1:f}"), i, Duration);
				return true;
			};
			time_no_lock = RunTestAndCalculateAvg(TestFunc, kNumIterations);
			HLVM_LOG(LogTest, info, TXT("No lock avg took {0:f}"), time_no_lock);
		}

		{
			HLVM_LOG(LogTest, info, TXT("With lock : Create 10 threads and adds to i"));
			auto TestFunc = [&](double& Duration) -> bool {
				int						 i = 0;
				FTimer					 Timer;
				FAtomicFlag				 lock;
				std::once_flag			 Flag;
				std::atomic<int>		 Counter{ kNumThreads };
				std::vector<std::thread> threads;
				FTimer					 timer{ true };
				for (int j = 0; j < kNumThreads; ++j)
				{
					threads.emplace_back([&i, &Timer, &Flag, &Counter, &Duration, &lock] {
						std::call_once(Flag, [&Timer] {
							Timer.Reset();
						});
						for (int k = 0; k < 1000; ++k)
						{
							ATOMIC_LOCK_GUARD(lock);
							i++;
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

				HLVM_LOG(LogTest, info, TXT("i = {0:d}, took {1:f}"), i, Duration);
				return true;
			};

			time_lock = RunTestAndCalculateAvg(TestFunc, kNumIterations);
			HLVM_LOG(LogTest, info, TXT("With lock avg took {0:f}"), time_lock);
		}
		double ratio = time_lock / time_no_lock;
		double efficient = kNumThreads / ratio * 100;
		HLVM_LOG(LogTest, info, TXT("No lock = {0:.2f}x With lock, lock is {1:.2f}% efficient"), ratio, efficient);
	})

RECORD(queue_test,
	{
		HLVM_LOG(LogTest, info, TXT("Queue test:"));

		constexpr int kNumThreads = 10;
		constexpr int kNumIterations = 10;
		double		  time_concurrent, time_lock;
		{
			HLVM_LOG(LogTest, info, TXT("Queue test #1 FConcurrentQueue"));
			auto Test1Func = [&](double& Duration) -> bool {
				auto					 Queue = FConcurrentQueue<int, EConcurrentQueueMode::Mpmc, true>();
				FTimer					 Timer;
				std::once_flag			 Flag;
				std::atomic<int>		 Counter{ kNumThreads };
				std::vector<std::thread> threads;
				for (int i = 0; i < kNumThreads; ++i)
				{
					threads.emplace_back([&Queue, &Timer, &Flag] {
						std::call_once(Flag, [&Timer] {
							Timer.Reset();
						});
						for (int j = 0; j < 1000; ++j)
						{
							Queue.Push(j);
						}
					});

					threads.emplace_back([&Queue, &Timer, &Counter, &Duration] {
						for (int j = 0; j < 1000;)
						{
							int val;
							if (Queue.PopFront(val))
							{
								++j;
							}
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
				// HLVM_LOG(LogTest, info, TXT("Queue test #1 took {0:f}, queue size {1:d}"), Duration, Queue.Num());
				return true;
			};

			time_concurrent = RunTestAndCalculateAvg(Test1Func, kNumIterations);
			HLVM_LOG(LogTest, info, TXT("Queue test #1 Concurrent avg took {0:f}, iter {1:d}"), time_concurrent, kNumIterations);
		}

		{
			HLVM_LOG(LogTest, info, TXT("Queue test #2 boost::lockfree::queue"));
			auto Test2Func = [&](double& Duration) -> bool {
				auto					 Queue = boost::lockfree::queue<int>(1000 * kNumThreads);
				FTimer					 Timer;
				std::once_flag			 Flag;
				std::atomic<int>		 Counter{ kNumThreads };
				std::vector<std::thread> threads;
				for (int i = 0; i < kNumThreads; ++i)
				{
					threads.emplace_back([&Queue, &Timer, &Flag] {
						std::call_once(Flag, [&Timer] {
							Timer.Reset();
						});
						for (int j = 0; j < 1000; ++j)
						{
							Queue.push(j);
						}
					});

					threads.emplace_back([&Queue, &Timer, &Counter, &Duration] {
						for (int j = 0; j < 1000;)
						{
							int val;
							if (Queue.pop(val))
							{
								j++;
							}
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
				// HLVM_LOG(LogTest, info, TXT("Queue test #2 took {0:f}, queue size {1:d}"), Duration, Queue.empty() ? 0 : -1);
				return true;
			};

			time_lock = RunTestAndCalculateAvg(Test2Func, kNumIterations);
			HLVM_LOG(LogTest, info, TXT("Queue test #2 boost::lockfree::queue avg took {0:f}, iter {1:d}"), time_lock, kNumIterations);
		}

		double ratio = time_lock / time_concurrent;
		double per_thread_gain = ratio / kNumThreads * 100;
		HLVM_LOG(LogTest, info, TXT("Queue test #1 = {0:.2f}x Queue test #2, per thread gain {1:.2f}%"), ratio, per_thread_gain);
	})