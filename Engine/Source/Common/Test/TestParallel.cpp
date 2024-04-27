/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Test.h"

#include "Core/Parallel/Lock.h"
#include "Core/Log.h"
#include "Utility/Timer.h"

#include <thread>
#include <vector>

#include <boost/fiber/detail/spinlock.hpp>
#include <boost/fiber/detail/spinlock_ttas.hpp>

DECLARE_LOG_CATEGORY(LogTest)

/*
	<test method>
*/
RECORD(lock_test, true)
{
	constexpr int kNumThreads = 10;
	constexpr int kNumIterations = 20;
	constexpr int kNumLoops = 10000;
	double		  time_no_lock, time_lock;
	{
		HLVM_LOG(LogTest, info, TXT("Atomic ops : Create 10 threads and adds to i"));
		auto TestFunc = [&](double& Duration) -> bool {
			std::atomic_int32_t		 i = 0;
			FTimer					 Timer;
			std::once_flag			 Flag;
			std::atomic_int_fast32_t Counter{ kNumThreads };
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
			//  std::optional<FAtomicFlag> lock; // test empty lock
			// std::optional<FAtomicFlag> lock = FAtomicFlag{}; // test non empty lock
			// std::optional<FRecursiveAtomicFlag> lock = FRecursiveAtomicFlag{}; // test recursive lock
			auto					 lock = FAtomicFlag();
			std::once_flag			 Flag;
			std::atomic_int_fast32_t Counter{ kNumThreads };
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
	{
		double ratio = time_lock / time_no_lock;
		double efficient = kNumThreads / ratio * 100;
		HLVM_LOG(LogTest, info,
			TXT("Atomic ops = {0:.2f}x With lock, lock is {1:.2f}% efficient, ideally, lock should be 95% to 99% efficient"),
			ratio, efficient);
	}
	{
		HLVM_LOG(LogTest, info, TXT("With lock : Create 10 threads and adds to i"));
		auto TestFunc = [&](double& Duration) -> bool {
			int						 i = 0;
			FTimer					 Timer;
			auto					 lock = boost::fibers::detail::spinlock_ttas{};
			std::once_flag			 Flag;
			std::atomic_int_fast32_t Counter{ kNumThreads };
			std::vector<std::thread> threads;
			for (int j = 0; j < kNumThreads; ++j)
			{
				threads.emplace_back([&i, &Timer, &Flag, &Counter, &Duration, &lock] {
					std::call_once(Flag, [&Timer] {
						Timer.Reset();
					});
					for (int k = 0; k < kNumLoops; ++k)
					{
						boost::fibers::detail::spinlock_lock lk{ lock };
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
		HLVM_LOG(LogTest, info, TXT("With boost spin lock avg took {0:f}"), time_lock);
	}
	{
		double ratio = time_lock / time_no_lock;
		double efficient = kNumThreads / ratio * 100;
		HLVM_LOG(LogTest, info,
			TXT("Atomic ops = {0:.2f}x With boost lock, boost lock is {1:.2f}% efficient, ideally, lock should be 95% to 99% efficient"),
			ratio, efficient);
	}
};

#include "Core/Parallel/ConcurrentQueue.h"
#include <boost/lockfree/queue.hpp>

RECORD(lock_free_queue_test, true)
{
	HLVM_PROFILER_CPU_ONOFF(false);
	HLVM_LOG(LogTest, info, TXT("Queue test:"));

	constexpr int kNumThreads = 10;
	constexpr int kNumIterations = 20;
	constexpr int kNumLoops = 10000;
	double		  time_concurrent, time_lock;
	{
		HLVM_LOG(LogTest, info, TXT("Queue test #1 TConcurrentQueue"));
		auto Test1Func = [&](double& Duration) -> bool {
			auto					 Queue = TConcurrentQueue<int, EConcurrentQueueMode::Mpmc, true>();
			FTimer					 Timer;
			std::once_flag			 Flag;
			std::atomic_int_fast32_t Counter{ kNumThreads };
			std::vector<std::thread> PushThreads;
			std::vector<std::thread> PopThreads;
			{
				Queue.Push(1);
				int& val = Queue.PeekFront();
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
			auto					 Queue = boost::lockfree::queue<int>(32);
			FTimer					 Timer;
			std::once_flag			 Flag;
			std::atomic_int_fast32_t Counter{ kNumThreads };
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

#include "Core/Parallel/FixedSizeQueue.h"
#include <boost/fiber/buffered_channel.hpp>

RECORD(fixed_queue_test, true)
{
	HLVM_PROFILER_CPU_ONOFF(false);
	HLVM_LOG(LogTest, info, TXT("Fixed Queue test:"));

	constexpr int kNumThreads = 10;
	constexpr int kNumIterations = 20;
	constexpr int kNumLoops = 10000;
	double		  time_concurrent, time_lock;
	{
		HLVM_LOG(LogTest, info, TXT("Queue test #1 TFixedSizeQueue"));
		auto Test1Func = [&](double& Duration) -> bool {
			auto					 Queue = TFixedSizeQueue<int, 32>();
			FTimer					 Timer;
			std::once_flag			 Flag;
			std::atomic_int_fast32_t J{ 0 };
			std::atomic_int_fast32_t Counter{ kNumThreads };
			std::vector<std::thread> PushThreads;
			std::vector<std::thread> PopThreads;
			{
				Queue.Push(1);
				int& val = Queue.PeekFront();
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
						HLVM_ENSURE(Queue.Push(j), TXT("Queue push failed"));
					}
				});

				PopThreads.emplace_back([&Queue, &Timer, &Counter, &Duration, &J] {
					for (; J < kNumLoops * kNumThreads;)
					{
						int val;
						if (Queue.PopFront<true>(val))
						{
							J.fetch_add(1, std::memory_order_relaxed);
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
			HLVM_LOG(LogTest, info, TXT("Fixed Queue test #1 took {0:f}, queue size {1:d}"), Duration, Queue.Num());
			return true;
		};

		time_concurrent = RunTestAndCalculateAvg(Test1Func, kNumIterations);
		HLVM_LOG(LogTest, info, TXT("Fixed Queue test #1 avg took {0:f}, iter {1:d}"), time_concurrent, kNumIterations);
	}

	{
		HLVM_LOG(LogTest, info, TXT("Fixed Queue test #2 boost::fiber::buffered_channel"));
		auto Test2Func = [&](double& Duration) -> bool {
			auto					 Queue = boost::fibers::buffered_channel<int>(32);
			FTimer					 Timer;
			std::once_flag			 Flag;
			std::atomic_int_fast32_t J{ 0 };
			std::atomic_int_fast32_t Counter{ kNumThreads };
			std::vector<std::thread> PushThreads;
			std::vector<std::thread> PopThreads;
			{
				Queue.push(1);
				int val;
				HLVM_ENSURE(Queue.pop(val) == boost::fibers::channel_op_status::success, TXT("Queue pop front failed"));
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

				PopThreads.emplace_back([&Queue, &Timer, &Counter, &Duration, &J] {
					for (; J < kNumLoops * kNumThreads;)
					{
						int val;
						if (Queue.try_pop(val) == boost::fibers::channel_op_status::success)
						{
							J.fetch_add(1, std::memory_order_relaxed);
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
			for (std::thread& t : PopThreads)
			{
				t.join();
			}
			HLVM_LOG(LogTest, info, TXT("Fixed Queue test #2 took {0:f}, queue size {1:d}"), Duration, J == kNumLoops * kNumThreads ? 0 : -1);
			return true;
		};

		time_lock = RunTestAndCalculateAvg(Test2Func, kNumIterations);
		HLVM_LOG(LogTest, info, TXT("Fixed Queue test #2 boost::fiber::buffered_channel avg took {0:f}, iter {1:d}"), time_lock, kNumIterations);
	}

	double ratio = time_lock / time_concurrent;
	HLVM_LOG(LogTest, info, TXT("Fixed Queue test #1 TFixedSizeQueue = {0:.2f}x faster than Fixed Queue test #2 boost::fiber::buffered_channel"), ratio);
};

#include "Core/Parallel/Async/WorkStealThreadPool.h"
#define HLVM_ENABLE_FIBER_POOL 0
#if HLVM_ENABLE_FIBER_POOL
	#include "Core/Parallel/Async/WorkStealFiberPool.h"
#else
	#include "Core/Parallel/Async/FiberPool.hpp"
#endif

RECORD(pool_test, true)
{
	HLVM_PROFILER_CPU_ONOFF(false);
	HLVM_LOG(LogTest, info, TXT("Pool test:"));

	constexpr int kNumThreads = 10;
	constexpr int kNumIterations = 20;
	constexpr int kNumLoops = 10000;
	double		  time_1, time_2, time_3 = 0;
	{
		HLVM_LOG(LogTest, info, TXT("Pool test #1 Thread"));
		auto Test1Func = [&](double& Duration) -> bool {
			std::atomic_int_fast32_t	   Number = 0;
			FTimer						   Timer;
			std::once_flag				   Flag;
			std::atomic_int_fast32_t	   Counter{ kNumThreads };
			std::vector<std::future<void>> PushThreads;
			std::vector<std::future<void>> PopThreads;
			FWorkStealThreadPool		   Pool{ FThreadAffinityMode::BgTwoPhysicalCores() };

			for (int i = 0; i < kNumThreads; ++i)
			{
				PushThreads.emplace_back(Pool.EnqueueTask([&Number, &Timer, &Flag] {
					std::call_once(Flag, [&Timer] {
						Timer.Reset();
					});
					for (int j = 0; j < kNumLoops; ++j)
					{
						Number.fetch_add(1, std::memory_order_relaxed);
					}
				}));

				PopThreads.emplace_back(Pool.EnqueueTask([&Number, &Timer, &Counter, &Duration] {
					for (int j = 0; j < kNumLoops; ++j)
					{
						Number.fetch_sub(1, std::memory_order_relaxed);
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
			for (auto& t : PopThreads)
			{
				t.wait();
			}
			HLVM_LOG(LogTest, info, TXT("Pool test #1 took {0:f}, queue size {1:d}"), Duration, Number.load());
			return true;
		};

		time_1 = RunTestAndCalculateAvg(Test1Func, kNumIterations);
		HLVM_LOG(LogTest, info, TXT("Pool test #1 ThreadPool avg took {0:f}, iter {1:d}"), time_1, kNumIterations);
	}

	{
		HLVM_LOG(LogTest, info, TXT("Pool test #2 Fiber"));
#if HLVM_ENABLE_FIBER_POOL
		auto Test2Func = [&](double& Duration) -> bool {
			std::atomic_int_fast32_t				 Number = 0;
			FTimer									 Timer;
			std::once_flag							 Flag;
			std::atomic_int_fast32_t				 Counter{ kNumThreads };
			std::vector<boost::fibers::future<void>> PushThreads;
			std::vector<boost::fibers::future<void>> PopThreads;
			FWorkStealFiberPool						 Pool{ FThreadAffinityMode::BgTwoPhysicalCores() };

			for (int i = 0; i < kNumThreads; ++i)
			{
				PushThreads.emplace_back(Pool.EnqueueTask([&Number, &Timer, &Flag] {
					std::call_once(Flag, [&Timer] {
						Timer.Reset();
					});
					for (int j = 0; j < kNumLoops; ++j)
					{
						Number.fetch_add(1, std::memory_order_relaxed);
					}
				}));

				PopThreads.emplace_back(Pool.EnqueueTask([&Number, &Timer, &Counter, &Duration] {
					for (int j = 0; j < kNumLoops; ++j)
					{
						Number.fetch_sub(1, std::memory_order_relaxed);
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
			for (auto& t : PopThreads)
			{
				t.wait();
			}
			HLVM_LOG(LogTest, info, TXT("Pool test #2 took {0:f}, queue size {1:d}"), Duration, Number.load());
			return true;
		};

		time_2 = RunTestAndCalculateAvg(Test2Func, kNumIterations);
		HLVM_LOG(LogTest, info, TXT("Pool test #2 Fiber avg took {0:f}, iter {1:d}"), time_2, kNumIterations);
#else
		auto Test2Func = [&](double& Duration) -> bool {
			std::atomic_int_fast32_t								Number = 0;
			FTimer													Timer;
			std::once_flag											Flag;
			std::atomic_int_fast32_t								Counter{ kNumThreads };
			std::vector<std::optional<boost::fibers::future<void>>> PushThreads;
			std::vector<std::optional<boost::fibers::future<void>>> PopThreads;
			auto													Pool = FiberPool::FiberPoolSharing<>{};
			for (int i = 0; i < kNumThreads; ++i)
			{
				PushThreads.emplace_back(Pool.submit([&Number, &Timer, &Flag] {
					std::call_once(Flag, [&Timer] {
						Timer.Reset();
					});
					for (int j = 0; j < kNumLoops; ++j)
					{
						Number.fetch_add(1, std::memory_order_relaxed);
					}
				}));

				PopThreads.emplace_back(Pool.submit([&Number, &Timer, &Counter, &Duration] {
					for (int j = 0; j < kNumLoops; ++j)
					{
						Number.fetch_sub(1, std::memory_order_relaxed);
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
			for (auto& t : PopThreads)
			{
				t->wait();
			}
			HLVM_LOG(LogTest, info, TXT("Pool test #2 took {0:f}, queue size {1:d}"), Duration, Number.load());
			return true;
		};

		time_2 = RunTestAndCalculateAvg(Test2Func, kNumIterations);
		HLVM_LOG(LogTest, info, TXT("Pool test #2 Fiber avg took {0:f}, iter {1:d}"), time_2, kNumIterations);
#endif
	}

	HLVM_LOG(LogTest, info, TXT("Pool Test Thread #1 = {0:.2f}x faster than Pool Test Fiber #2"), time_2 / time_1);

	{
		HLVM_LOG(LogTest, info, TXT("Pool test #3 Async"));
		auto Test3Func = [&](double& Duration) -> bool {
			std::atomic_int_fast32_t	   Number = 0;
			FTimer						   Timer;
			std::once_flag				   Flag;
			std::atomic_int_fast32_t	   Counter{ kNumThreads };
			std::vector<std::future<void>> PushThreads;
			std::vector<std::future<void>> PopThreads;

			for (int i = 0; i < kNumThreads; ++i)
			{
				PushThreads.emplace_back(std::async(std::launch::async, [&Number, &Timer, &Flag] {
					std::call_once(Flag, [&Timer] {
						Timer.Reset();
					});
					for (int j = 0; j < kNumLoops; ++j)
					{
						Number.fetch_add(1, std::memory_order_relaxed);
					}
				}));

				PopThreads.emplace_back(std::async(std::launch::async, [&Number, &Timer, &Counter, &Duration] {
					for (int j = 0; j < kNumLoops; ++j)
					{
						Number.fetch_sub(1, std::memory_order_relaxed);
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
			for (auto& t : PopThreads)
			{
				t.wait();
			}
			HLVM_LOG(LogTest, info, TXT("Pool test #3 took {0:f}, queue size {1:d}"), Duration, Number.load());
			return true;
		};

		time_3 = RunTestAndCalculateAvg(Test3Func, kNumIterations);
		HLVM_LOG(LogTest, info, TXT("Pool test #3 ThreadPool avg took {0:f}, iter {1:d}"), time_3, kNumIterations);
	}

	HLVM_LOG(LogTest, info, TXT("Pool Test Thread #1 = {0:.2f}x faster than Pool Test Aync #3"), time_3 / time_1);
};
