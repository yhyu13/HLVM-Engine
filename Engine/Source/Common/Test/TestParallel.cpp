/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Test.h"

#include "Core/Parallel/Lock.h"
#include "Core/Parallel/ConcurrentQueue.h"
#include "Core/Log.h"
#include "Utility/Timer.h"

#include <thread>
#include <vector>
#include <boost/lockfree/queue.hpp>

DELCARE_LOG_CATEGORY(LogTest)

/*
	<test method>
*/
RECORD(lock_test, true)
{
	constexpr int kNumThreads = 10;
	constexpr int kNumIterations = 50;
	constexpr int kNumLoops = 10000;
	double		  time_no_lock, time_lock;
	{
		HLVM_LOG(LogTest, info, TXT("Atomic ops : Create 10 threads and adds to i"));
		auto TestFunc = [&](double& Duration) -> bool {
			std::atomic_int32_t		 i = 0;
			FTimer					 Timer;
			std::once_flag			 Flag;
			std::atomic<int>		 Counter{ kNumThreads };
			std::vector<std::thread> threads;
			for (int j = 0; j < kNumThreads; ++j)
			{
				threads.emplace_back([&i, &Timer, &Flag, &Counter, &Duration] {
					std::call_once(Flag, [&Timer] {
						Timer.Reset();
					});
					for (int k = 0; k < kNumLoops; ++k)
					{
						i.fetch_add(1, std::memory_order_relaxed);
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

			HLVM_LOG(LogTest, info, TXT("i = {0:d}, took {1:f}"), i.load(), Duration);
			return true;
		};
		time_no_lock = RunTestAndCalculateAvg(TestFunc, kNumIterations);
		HLVM_LOG(LogTest, info, TXT("Atomic ops avg took {0:f}"), time_no_lock);
	}

	{
		HLVM_LOG(LogTest, info, TXT("With lock : Create 10 threads and adds to i"));
		auto TestFunc = [&](double& Duration) -> bool {
			int	   i = 0;
			FTimer Timer;
			// std::optional<FAtomicFlag> lock = FAtomicFlag{};
			std::optional<FRecursiveAtomicFlag> lock = FRecursiveAtomicFlag{};
			//  std::optional<FAtomicFlag> lock;
			std::once_flag			 Flag;
			std::atomic<int>		 Counter{ kNumThreads };
			std::vector<std::thread> threads;
			for (int j = 0; j < kNumThreads; ++j)
			{
				threads.emplace_back([&i, &Timer, &Flag, &Counter, &Duration, &lock] {
					std::call_once(Flag, [&Timer] {
						Timer.Reset();
					});
					for (int k = 0; k < kNumLoops; ++k)
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
	HLVM_LOG(LogTest, info, TXT("Atomic ops = {0:.2f}x With lock, lock is {1:.2f}% efficient, ideally, lock should be 95% to 99% efficient"), ratio, efficient);
};

RECORD(queue_test, true)
{
	HLVM_LOG(LogTest, info, TXT("Queue test:"));

	constexpr int kNumThreads = 10;
	constexpr int kNumIterations = 50;
	constexpr int kNumLoops = 10000;
	double		  time_concurrent, time_lock;
	{
		HLVM_LOG(LogTest, info, TXT("Queue test #1 TConcurrentQueue"));
		auto Test1Func = [&](double& Duration) -> bool {
			auto					 Queue = TConcurrentQueue<int, EConcurrentQueueMode::Mpmc, true, true>();
			FTimer					 Timer;
			std::once_flag			 Flag;
			std::atomic<int>		 Counter{ kNumThreads };
			std::vector<std::thread> PushThreads;
			std::vector<std::thread> PopThreads;
			{
				Queue.Push(1);
				int val = Queue.PeekFront();
				HLVM_ENSURE(val == 1, TXT("Queue peek front failed"));
				HLVM_ENSURE(Queue.PopFront(val), TXT("Queue pop front failed"));
			}
			for (int i = 0; i < kNumThreads; ++i)
			{
				PushThreads.emplace_back([&Queue, &Timer, &Flag] {
					std::call_once(Flag, [&Timer] {
						Timer.Reset();
					});
					for (int j = 0; j < kNumLoops; ++j)
					{
						Queue.Push(j);
					}
				});

				PopThreads.emplace_back([&Queue, &Timer, &Counter, &Duration] {
					for (int j = 0; !Queue.ShouldStopPop();)
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

			for (std::thread& t : PushThreads)
			{
				t.join();
			}
			Queue.SignalStop();
			for (std::thread& t : PopThreads)
			{
				t.join();
			}
			HLVM_LOG(LogTest, info, TXT("Queue test #1 took {0:f}, queue size {1:d}"), Duration, Queue.Num());
			return true;
		};

		time_concurrent = RunTestAndCalculateAvg(Test1Func, kNumIterations);
		HLVM_LOG(LogTest, info, TXT("Queue test #1 Concurrent avg took {0:f}, iter {1:d}"), time_concurrent, kNumIterations);
	}

	{
		HLVM_LOG(LogTest, info, TXT("Queue test #2 boost::lockfree::queue"));
		auto Test2Func = [&](double& Duration) -> bool {
			auto					 Queue = boost::lockfree::queue<int>(kNumLoops * kNumThreads);
			FTimer					 Timer;
			std::once_flag			 Flag;
			std::atomic<int>		 Counter{ kNumThreads };
			bool					 bSignalStop = false;
			std::vector<std::thread> PushThreads;
			std::vector<std::thread> PopThreads;
			{
				Queue.push(1);
				int val;
				HLVM_ENSURE(Queue.pop(val), TXT("Queue pop front failed"));
				HLVM_ENSURE(val == 1, TXT("Queue peek front failed"));
			}
			for (int i = 0; i < kNumThreads; ++i)
			{
				PushThreads.emplace_back([&Queue, &Timer, &Flag] {
					std::call_once(Flag, [&Timer] {
						Timer.Reset();
					});
					for (int j = 0; j < kNumLoops; ++j)
					{
						Queue.push(j);
					}
				});

				PopThreads.emplace_back([&Queue, &Timer, &Counter, &Duration, &bSignalStop] {
					for (int j = 0; !(Queue.empty() && bSignalStop);)
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

			for (std::thread& t : PushThreads)
			{
				t.join();
			}
			bSignalStop = true;
			for (std::thread& t : PopThreads)
			{
				t.join();
			}
			HLVM_LOG(LogTest, info, TXT("Queue test #2 took {0:f}, queue size {1:d}"), Duration, Queue.empty() ? 0 : -1);
			return true;
		};

		time_lock = RunTestAndCalculateAvg(Test2Func, kNumIterations);
		HLVM_LOG(LogTest, info, TXT("Queue test #2 boost::lockfree::queue avg took {0:f}, iter {1:d}"), time_lock, kNumIterations);
	}

	double ratio = time_lock / time_concurrent;
	HLVM_LOG(LogTest, info, TXT("Queue test #1 TConcurrentQueue = {0:.2f}x faster than Queue test #2 boost::lockfree::queue"), ratio);
};

#include "Core/Parallel/Async/WorkStealThreadPool.h"
#define HLVM_ENABLE_FIBER_POOL 0
#include "Core/Parallel/Async/WorkStealFiberPool.h"

#if !HLVM_ENABLE_FIBER_POOL
	#include "Core/Parallel/Async/FiberPool.hpp"
// #include "Core/Parallel/Async/FiberPool2.hpp"
// #include "Core/Parallel/Async/FiberPool2_1.hpp"
// #include "Core/Parallel/Async/FiberPool3.hpp"
// #include "Core/Parallel/Async/FiberPool4.hpp"
#endif

RECORD(pool_test)
{
	HLVM_LOG(LogTest, info, TXT("Pool test:"));

	constexpr int kNumThreads = 10;
	constexpr int kNumIterations = 50;
	constexpr int kNumLoops = 10000;
	double		  time_concurrent, time_lock = 0;
	{
		HLVM_LOG(LogTest, info, TXT("Pool test #1 Thread"));
		auto Test1Func = [&](double& Duration) -> bool {
			auto						   Queue = TConcurrentQueue<int, EConcurrentQueueMode::Mpmc, true, true>();
			FTimer						   Timer;
			std::once_flag				   Flag;
			std::atomic<int>			   Counter{ kNumThreads };
			std::vector<std::future<void>> PushThreads;
			std::vector<std::future<void>> PopThreads;
			FWorkStealThreadPool		   Pool{ BgTwoPhysicalCores };

			{
				Queue.Push(1);
				int val = Queue.PeekFront();
				HLVM_ENSURE(val == 1, TXT("Queue peek front failed"));
				HLVM_ENSURE(Queue.PopFront(val), TXT("Queue pop front failed"));
			}
			for (int i = 0; i < kNumThreads; ++i)
			{
				PushThreads.emplace_back(Pool.EnqueuTask([&Queue, &Timer, &Flag] {
					std::call_once(Flag, [&Timer] {
						Timer.Reset();
					});
					for (int j = 0; j < kNumLoops; ++j)
					{
						Queue.Push(j);
					}
				}));

				PopThreads.emplace_back(Pool.EnqueuTask([&Queue, &Timer, &Counter, &Duration] {
					for (int j = 0; !Queue.ShouldStopPop();)
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
				}));
			}

			for (auto& t : PushThreads)
			{
				t.wait();
			}
			Queue.SignalStop();
			for (auto& t : PopThreads)
			{
				t.wait();
			}
			HLVM_LOG(LogTest, info, TXT("Pool test #1 took {0:f}, queue size {1:d}"), Duration, Queue.Num());
			return true;
		};

		time_concurrent = RunTestAndCalculateAvg(Test1Func, kNumIterations);
		HLVM_LOG(LogTest, info, TXT("Pool test #1 ThreadPool avg took {0:f}, iter {1:d}"), time_concurrent, kNumIterations);
	}

	{
		HLVM_LOG(LogTest, info, TXT("Pool test #2 Fiber"));
#if HLVM_ENABLE_FIBER_POOL
		auto Test2Func = [&](double& Duration) -> bool {
			auto									 Queue = TConcurrentQueue<int, EConcurrentQueueMode::Mpmc, true, true>();
			FTimer									 Timer;
			std::once_flag							 Flag;
			std::atomic<int>						 Counter{ kNumThreads };
			std::vector<boost::fibers::future<void>> PushThreads;
			std::vector<boost::fibers::future<void>> PopThreads;
			FWorkStealFiberPool						 Pool{ BgTwoPhysicalCores };
			{
				Queue.Push(1);
				int val = Queue.PeekFront();
				HLVM_ENSURE(val == 1, TXT("Queue peek front failed"));
				HLVM_ENSURE(Queue.PopFront(val), TXT("Queue pop front failed"));
			}
			for (int i = 0; i < kNumThreads; ++i)
			{
				PushThreads.emplace_back(Pool.EnqueuTask([&Queue, &Timer, &Flag] {
					std::call_once(Flag, [&Timer] {
						Timer.Reset();
					});
					for (int j = 0; j < kNumLoops; ++j)
					{
						Queue.Push(j);
					}
				}));

				PopThreads.emplace_back(Pool.EnqueuTask([&Queue, &Timer, &Counter, &Duration] {
					for (int j = 0; !Queue.ShouldStopPop();)
					{
						int val;
						if (Queue.PopFront(val))
						{
							++j;
						}
						else
						{
							boost::this_thread::yield();
							boost::this_fiber::yield();
						}
					}
					if (--Counter == 0)
					{
						Duration = Timer.Mark();
					}
				}));
			}

			for (auto& t : PushThreads)
			{
				t.wait();
			}
			Queue.SignalStop();
			for (auto& t : PopThreads)
			{
				t.wait();
			}
			HLVM_LOG(LogTest, info, TXT("Pool test #2 took {0:f}, queue size {1:d}"), Duration, Queue.Num());
			return true;
		};

		time_lock = RunTestAndCalculateAvg(Test2Func, kNumIterations);
		HLVM_LOG(LogTest, info, TXT("Pool test #2 Fiber avg took {0:f}, iter {1:d}"), time_lock, kNumIterations);
#else
		auto Test2Func = [&](double& Duration) -> bool {
			auto													Queue = TConcurrentQueue<int, EConcurrentQueueMode::Mpmc, true, true>();
			FTimer													Timer;
			std::once_flag											Flag;
			std::atomic<int>										Counter{ kNumThreads };
			std::vector<std::optional<boost::fibers::future<void>>> PushThreads;
			std::vector<std::optional<boost::fibers::future<void>>> PopThreads;
			auto													Pool = FiberPool::FiberPoolSharing<>{};
			{
				Queue.Push(1);
				int val = Queue.PeekFront();
				HLVM_ENSURE(val == 1, TXT("Queue peek front failed"));
				HLVM_ENSURE(Queue.PopFront(val), TXT("Queue pop front failed"));
			}
			for (int i = 0; i < kNumThreads; ++i)
			{
				PushThreads.emplace_back(Pool.submit([&Queue, &Timer, &Flag] {
					std::call_once(Flag, [&Timer] {
						Timer.Reset();
					});
					for (int j = 0; j < kNumLoops; ++j)
					{
						Queue.Push(j);
					}
				}));

				PopThreads.emplace_back(Pool.submit([&Queue, &Timer, &Counter, &Duration] {
					for (int j = 0; !Queue.ShouldStopPop();)
					{
						int val;
						if (Queue.PopFront(val))
						{
							++j;
						}
						else
						{
							boost::this_thread::yield();
							boost::this_fiber::yield();
						}
					}
					if (--Counter == 0)
					{
						Duration = Timer.Mark();
					}
				}));
			}

			for (auto& t : PushThreads)
			{
				t->wait();
			}
			Queue.SignalStop();
			for (auto& t : PopThreads)
			{
				t->wait();
			}
			HLVM_LOG(LogTest, info, TXT("Pool test #2 took {0:f}, queue size {1:d}"), Duration, Queue.Num());
			return true;
		};

		time_lock = RunTestAndCalculateAvg(Test2Func, kNumIterations);
		HLVM_LOG(LogTest, info, TXT("Pool test #2 Fiber avg took {0:f}, iter {1:d}"), time_lock, kNumIterations);
#endif
	}

	double ratio = time_lock / time_concurrent;
	HLVM_LOG(LogTest, info, TXT("Pool Test Thread #1 = {0:.2f}x faster than Pool Test Fiber #2"), ratio);
};
