/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Test.h"
// Use third party taskflow giving advantage of latest feature & bug fixes (subtle) that vcpkg stable version cannot provide
#define USE_THIRD_PARTY_TF 1
#if USE_THIRD_PARTY_TF
	#include "ThirdParty/TaskFlow.h"
#else
	#include <taskflow/taskflow.hpp>
	#include <taskflow/algorithm/for_each.hpp>
	#include <taskflow/algorithm/transform.hpp>
	#include <taskflow/algorithm/reduce.hpp>
	#include <taskflow/algorithm/sort.hpp>
#endif

DECLARE_LOG_CATEGORY(LogTest)

// https://github.com/taskflow/taskflow/blob/master/examples/async.cpp
RECORD_INT(taskflow_async_test)
{
	HLVM_PROFILE_CPU_NAMED("taskflow_async_test");

	HLVM_LOG(LogTest, info, TXT("Test taskflow_async_test!"));
	{
		tf::Executor executor{ 4 };

		// create asynchronous tasks from the executor
		// (using executor as a thread pool)
		std::future<int> fu = executor.async([]() {
			std::cout << "async task 1 returns 1\n";
			return 1;
		});

		executor.silent_async([]() { // silent async task doesn't return any future object
			std::cout << "async task 2 does not return (silent)\n";
		});

		executor.wait_for_all(); // wait for the two async tasks to finish

		// create asynchronous tasks from a subflow
		// all asynchronous tasks are guaranteed to finish when the subflow joins
		tf::Taskflow taskflow;

		std::atomic<int> counter{ 0 };

		taskflow.emplace([&](tf::Subflow& sf) {
			for (int i = 0; i < 100; i++)
			{
				sf.silent_async([&]() { counter.fetch_add(1, std::memory_order_relaxed); });
			}
			sf.join();

			// when subflow joins, all spawned tasks from the subflow will finish
			if (counter == 100)
			{
				std::cout << "async tasks spawned from the subflow all finish\n";
			}
			else
			{
				throw std::runtime_error("this should not happen");
			}
		});

		executor.run(taskflow).wait();

		return 0;
	}
}

// https://github.com/taskflow/taskflow/blob/master/examples/attach_data.cpp
RECORD_INT(taskflow_data_test)
{
	HLVM_PROFILE_CPU_NAMED("taskflow_data_test");

	HLVM_LOG(LogTest, info, TXT("Test taskflow_data_test!"));
	{
		tf::Executor executor{ 4 };
		tf::Taskflow taskflow("attach data to a task");

		int data;

		// create a task and attach it the data
		auto A = taskflow.placeholder();
		A.data(&data).work([A]() {
			auto d = *static_cast<int*>(A.data());
			std::cout << "data is " << d << std::endl;
		});

		// run the taskflow iteratively with changing data
		for (data = 0; data < 10; data++)
		{
			executor.run(taskflow).wait();
		}

		return 0;
	}
}

// https://github.com/taskflow/taskflow/blob/master/examples/cancel.cpp
RECORD_INT(taskflow_cancel_test)
{
	HLVM_PROFILE_CPU_NAMED("taskflow_cancel_test");

	HLVM_LOG(LogTest, info, TXT("Test taskflow_cancel_test!"));
	{
		tf::Executor executor{ 4 };
		tf::Taskflow taskflow("cancel");

		// We create a taskflow graph of 1000 tasks each of 1 second.
		// Ideally, the taskflow completes in 1000/P seconds, where P
		// is the number of workers.
		for (int i = 0; i < 1000; i++)
		{
			taskflow.emplace([]() {
				std::this_thread::sleep_for(std::chrono::seconds(1));
			});
		}

		// submit the taskflow
		auto			 beg = std::chrono::steady_clock::now();
		tf::Future<void> fu = executor.run(taskflow);

		// submit a cancel request to cancel all 1000 tasks.
		fu.cancel();

		// wait until the cancellation finishes
		fu.get();
		auto end = std::chrono::steady_clock::now();

		// the duration should be much less than 1000 seconds
		std::cout << "taskflow completes in "
				  << std::chrono::duration_cast<std::chrono::milliseconds>(end - beg).count()
				  << " milliseconds\n";

		return 0;
	}
}

// https://github.com/taskflow/taskflow/blob/master/examples/composition.cpp
static void composition_example_1()
{

	std::cout << "Composition example 1\n";

	tf::Executor executor{ 4 };

	// f1 has three independent tasks
	tf::Taskflow f1("F1");
	auto		 f1A = f1.emplace([]() { std::cout << "F1 TaskA\n"; });
	auto		 f1B = f1.emplace([]() { std::cout << "F1 TaskB\n"; });
	auto		 f1C = f1.emplace([]() { std::cout << "F1 TaskC\n"; });
	f1A.name("f1A");
	f1B.name("f1B");
	f1C.name("f1C");
	f1A.precede(f1C);
	f1B.precede(f1C);

	// f2A ---
	//        |----> f2C ----> f1_module_task ----> f2D
	// f2B ---
	tf::Taskflow f2("F2");
	auto		 f2A = f2.emplace([]() { std::cout << "  F2 TaskA\n"; });
	auto		 f2B = f2.emplace([]() { std::cout << "  F2 TaskB\n"; });
	auto		 f2C = f2.emplace([]() { std::cout << "  F2 TaskC\n"; });
	auto		 f2D = f2.emplace([]() { std::cout << "  F2 TaskD\n"; });
	f2A.name("f2A");
	f2B.name("f2B");
	f2C.name("f2C");
	f2D.name("f2D");

	f2A.precede(f2C);
	f2B.precede(f2C);

	tf::Task f1_module_task = f2.composed_of(f1);
	f1_module_task.name("module");
	f2C.precede(f1_module_task);
	f1_module_task.precede(f2D);

	f2.dump(std::cout);

	executor.run_n(f2, 3).get();
}

static void composition_example_2()
{

	std::cout << "Composition example 2\n";

	tf::Executor executor{ 4 };

	// f1 has two independent tasks
	tf::Taskflow f1("F1");
	auto		 f1A = f1.emplace([&]() { std::cout << "F1 TaskA\n"; });
	auto		 f1B = f1.emplace([&]() { std::cout << "F1 TaskB\n"; });
	f1A.name("f1A");
	f1B.name("f1B");

	//  f2A ---
	//         |----> f2C
	//  f2B ---
	//
	//  f1_module_task
	tf::Taskflow f2("F2");
	auto		 f2A = f2.emplace([&]() { std::cout << "  F2 TaskA\n"; });
	auto		 f2B = f2.emplace([&]() { std::cout << "  F2 TaskB\n"; });
	auto		 f2C = f2.emplace([&]() { std::cout << "  F2 TaskC\n"; });
	f2A.name("f2A");
	f2B.name("f2B");
	f2C.name("f2C");

	f2A.precede(f2C);
	f2B.precede(f2C);
	f2.composed_of(f1).name("module_of_f1");

	// f3 has a module task (f2) and a regular task
	tf::Taskflow f3("F3");
	f3.composed_of(f2).name("module_of_f2");
	f3.emplace([]() { std::cout << "      F3 TaskA\n"; }).name("f3A");

	// f4: f3_module_task -> f2_module_task
	tf::Taskflow f4;
	f4.name("F4");
	auto f3_module_task = f4.composed_of(f3).name("module_of_f3");
	auto f2_module_task = f4.composed_of(f2).name("module_of_f2");
	f3_module_task.precede(f2_module_task);

	f4.dump(std::cout);

	executor.run_until(
				f4,
				[iter = 1]() mutable { std::cout << '\n'; return iter-- == 0; },
				[]() { std::cout << "First run_until finished\n"; })
		.get();

	executor.run_until(
		f4,
		[iter = 2]() mutable { std::cout << '\n'; return iter-- == 0; },
		[]() { std::cout << "Second run_until finished\n"; });

	executor.run_until(
				f4,
				[iter = 3]() mutable { std::cout << '\n'; return iter-- == 0; },
				[]() { std::cout << "Third run_until finished\n"; })
		.get();
}

RECORD_INT(taskflow_composition_test)
{
	HLVM_PROFILE_CPU_NAMED("taskflow_composition_test");

	HLVM_LOG(LogTest, info, TXT("Test taskflow_composition_test!"));
	{
		composition_example_1();
		composition_example_2();
		return 0;
	}
}

// https://github.com/taskflow/taskflow/blob/master/examples/condition.cpp
RECORD_INT(taskflow_condition_test)
{
	HLVM_PROFILE_CPU_NAMED("taskflow_condition_test");

	HLVM_LOG(LogTest, info, TXT("Test taskflow_condition_test!"));
	{
		tf::Executor executor{ 4 };
		tf::Taskflow taskflow("Conditional Tasking Demo");

		int counter;

		auto A = taskflow.emplace([&]() {
							 std::cout << "initializes the counter to zero\n";
							 counter = 0;
						 })
					 .name("A");

		auto B = taskflow.emplace([&]() {
							 std::cout << "loops to increment the counter\n";
							 counter++;
						 })
					 .name("B");

		auto C = taskflow.emplace([&]() {
							 std::cout << "counter is " << counter << " -> ";
							 if (counter != 5)
							 {
								 std::cout << "loops again (goes to B)\n";
								 return 0;
							 }
							 std::cout << "breaks the loop (goes to D)\n";
							 return 1;
						 })
					 .name("C");

		auto D = taskflow.emplace([&]() {
							 std::cout << "done with counter equal to " << counter << '\n';
						 })
					 .name("D");

		A.precede(B);
		B.precede(C);
		C.precede(B);
		C.precede(D);

		// visualizes the taskflow
		taskflow.dump(std::cout);

		// executes the taskflow
		executor.run(taskflow).wait();

		assert(counter == 5);

		return 0;
	}
}

// https://github.com/taskflow/taskflow/blob/master/examples/corun.cpp
RECORD_INT(taskflow_corun_test)
{
	HLVM_PROFILE_CPU_NAMED("taskflow_corun_test");

	HLVM_LOG(LogTest, info, TXT("Test taskflow_corun_test!"));
	{
		const size_t N = 100;
		const size_t T = 1000;

		// create an executor and a taskflow
		tf::Executor executor(2);
		tf::Taskflow taskflow;

		std::array<tf::Taskflow, N> taskflows;

		std::atomic<size_t> counter{ 0 };

		for (size_t n = 0; n < N; n++)
		{
			for (size_t i = 0; i < T; i++)
			{
				taskflows[n].emplace([&]() { counter++; });
			}
			taskflow.emplace([&executor, &tf = taskflows[n]]() {
				executor.corun(tf);
				// executor.run(tf).wait();  <-- can result in deadlock
			});
		}

		executor.run(taskflow).wait();

		return 0;
	}
}

// https://github.com/taskflow/taskflow/blob/master/examples/dependent_async.cpp
RECORD_INT(taskflow_dependent_async_test)
{
	HLVM_PROFILE_CPU_NAMED("taskflow_dependent_async_test");

	HLVM_LOG(LogTest, info, TXT("Test taskflow_dependent_async_test!"));
	{
		tf::Executor executor{ 4 };

		// demonstration of dependent async (with future)
		printf("Dependent Async\n");
		auto [A, fuA] = executor.dependent_async([]() { printf("A\n"); });
		auto [B, fuB] = executor.dependent_async([]() { printf("B\n"); }, A);
		auto [C, fuC] = executor.dependent_async([]() { printf("C\n"); }, A);
		auto [D, fuD] = executor.dependent_async([]() { printf("D\n"); }, B, C);

		fuD.get();

		// demonstration of silent dependent async (without future)
		printf("Silent Dependent Async\n");
		A = executor.silent_dependent_async([]() { printf("A\n"); });
		B = executor.silent_dependent_async([]() { printf("B\n"); }, A);
		C = executor.silent_dependent_async([]() { printf("C\n"); }, A);
		D = executor.silent_dependent_async([]() { printf("D\n"); }, B, C);

		executor.wait_for_all();

		return 0;
	}
}

// https://github.com/taskflow/taskflow/blob/master/examples/dependent_async_algorithm.cpp

RECORD_INT(taskflow_dependent_async_algorithm_test)
{
	HLVM_PROFILE_CPU_NAMED("taskflow_dependent_async_algorithm_test");

	HLVM_LOG(LogTest, info, TXT("Test taskflow_dependent_async_algorithm_test!"));
	{
		const size_t N = 65536;

		tf::Executor executor{ 4 };

		int				 sum{ 1 };
		std::vector<int> data(N);

		// for-each
		tf::AsyncTask A = executor.silent_dependent_async(tf::make_for_each_task(
			data.begin(), data.end(), [](int& i) { i = 1; }));

		// transform
		tf::AsyncTask B = executor.silent_dependent_async(tf::make_transform_task(
															  data.begin(), data.end(), data.begin(), [](int& i) { return i * 2; }),
			A);

		// reduce
		tf::AsyncTask C = executor.silent_dependent_async(tf::make_reduce_task(
															  data.begin(), data.end(), sum, std::plus<int>{}),
			B);

		// wait for all async task to complete
		executor.wait_for_all();

		// verify the result
		if (sum != N * 2 + 1)
		{
			throw std::runtime_error("INCORRECT RESULT");
		}
		else
		{
			std::cout << "CORRECT RESULT\n";
		}

		return 0;
	}
}

// https://github.com/taskflow/taskflow/blob/master/examples/do_while_loop.cpp
RECORD_INT(taskflow_do_while_loop_test)
{
	HLVM_PROFILE_CPU_NAMED("taskflow_do_while_loop_test");

	HLVM_LOG(LogTest, info, TXT("Test taskflow_do_while_loop_test!"));
	{
		tf::Executor executor{ 4 };
		tf::Taskflow taskflow;

		int i;

		auto [init, body, cond, done] = taskflow.emplace(
			[&]() { std::cout << "i=0\n"; i=0; },
			[&]() { std::cout << "i++ => i="; i++; },
			[&]() { std::cout << i << '\n'; return i<5 ? 0 : 1; },
			[&]() { std::cout << "done\n"; });

		init.name("init");
		body.name("do i++");
		cond.name("while i<5");
		done.name("done");

		init.precede(body);
		body.precede(cond);
		cond.precede(body, done);

		// taskflow.dump(std::cout);

		executor.run(taskflow).wait();

		return 0;
	}
}

// https://github.com/taskflow/taskflow/blob/master/examples/exception.cpp
RECORD_INT(taskflow_exception_test)
{
	HLVM_PROFILE_CPU_NAMED("taskflow_exception_test");

	HLVM_LOG(LogTest, info, TXT("Test taskflow_exception_test!"));
	{
		tf::Executor executor{ 4 };
		tf::Taskflow taskflow("exception");

		auto [A, B, C, D] = taskflow.emplace(
			[]() { std::cout << "TaskA\n"; },
			[]() {
				std::cout << "TaskB\n";
				throw std::runtime_error("Exception on Task B");
			},
			[]() {
				std::cout << "TaskC\n";
				throw std::runtime_error("Exception on Task C");
			},
			[]() { std::cout << "TaskD will not be printed due to exception\n"; });

		A.precede(B, C); // A runs before B and C
		D.succeed(B, C); // D runs after  B and C

		try
		{
			executor.run(taskflow).get();
		}
		catch (const std::runtime_error& e)
		{
			// catched either TaskB's or TaskC's exception
			std::cout << e.what() << std::endl;
		}

		return 0;
	}
}

// https://github.com/taskflow/taskflow/blob/master/examples/if_else.cpp
RECORD_INT(taskflow_if_else_test)
{
	HLVM_PROFILE_CPU_NAMED("taskflow_if_else_test");

	HLVM_LOG(LogTest, info, TXT("Test taskflow_if_else_test!"));
	{
		tf::Executor executor{ 4 };
		tf::Taskflow taskflow;

		// create three static tasks and one condition task
		auto [init, cond, yes, no] = taskflow.emplace(
			[]() {},
			[]() { return 0; },
			[]() { std::cout << "yes\n"; },
			[]() { std::cout << "no\n"; });

		init.name("init");
		cond.name("cond");
		yes.name("yes");
		no.name("no");

		cond.succeed(init);

		// With this order, when cond returns 0, execution
		// moves on to yes. When cond returns 1, execution
		// moves on to no.
		cond.precede(yes, no);

		// dump the conditioned flow
		taskflow.dump(std::cout);

		executor.run(taskflow).wait();

		return 0;
	}
}

// https://github.com/taskflow/taskflow/blob/master/examples/multi_condition.cpp
RECORD_INT(taskflow_multi_condition_test)
{
	HLVM_PROFILE_CPU_NAMED("taskflow_multi_condition_test");

	HLVM_LOG(LogTest, info, TXT("Test taskflow_multi_condition_test!"));
	{
		tf::Executor executor{ 4 };
		tf::Taskflow taskflow("Multi-Conditional Tasking Demo");

		auto A = taskflow.emplace([&]() -> tf::SmallVector<int> {
							 std::cout << "A\n";
							 return { 0, 2 };
						 })
					 .name("A");
		auto B = taskflow.emplace([&]() { std::cout << "B\n"; }).name("B");
		auto C = taskflow.emplace([&]() { std::cout << "C\n"; }).name("C");
		auto D = taskflow.emplace([&]() { std::cout << "D\n"; }).name("D");

		A.precede(B, C, D);

		// visualizes the taskflow
		taskflow.dump(std::cout);

		// executes the taskflow
		executor.run(taskflow).wait();

		return 0;
	}
}

// https://github.com/taskflow/taskflow/blob/master/examples/nested_if_else.cpp
RECORD_INT(taskflow_nested_if_else_test)
{
	HLVM_PROFILE_CPU_NAMED("taskflow_nested_if_else_test");

	HLVM_LOG(LogTest, info, TXT("Test taskflow_nested_if_else_test!"));
	{
		tf::Executor executor{ 4 };
		tf::Taskflow taskflow;

		int i;

		// create three condition tasks for nested control flow
		auto initi = taskflow.emplace([&]() { i = 3; });
		auto cond1 = taskflow.emplace([&]() { return i > 1 ? 1 : 0; });
		auto cond2 = taskflow.emplace([&]() { return i > 2 ? 1 : 0; });
		auto cond3 = taskflow.emplace([&]() { return i > 3 ? 1 : 0; });
		auto equl1 = taskflow.emplace([&]() { std::cout << "i=1\n"; });
		auto equl2 = taskflow.emplace([&]() { std::cout << "i=2\n"; });
		auto equl3 = taskflow.emplace([&]() { std::cout << "i=3\n"; });
		auto grtr3 = taskflow.emplace([&]() { std::cout << "i>3\n"; });

		initi.precede(cond1);
		cond1.precede(equl1, cond2);
		cond2.precede(equl2, cond3);
		cond3.precede(equl3, grtr3);

		// dump the conditioned flow
		taskflow.dump(std::cout);

		executor.run(taskflow).wait();

		return 0;
	}
}

// https://github.com/taskflow/taskflow/blob/master/examples/parallel_for.cpp
// Procedure: for_each
static void for_each(size_t N)
{

	tf::Executor executor{ 4 };
	tf::Taskflow taskflow;

	std::vector<size_t> range(N);
	std::iota(range.begin(), range.end(), 0);

	taskflow.for_each(range.begin(), range.end(), [&](size_t) {
		// printf("for_each on container item: %zu\n", i);
	});

	executor.run(taskflow).get();

	taskflow.dump(std::cout);
}

// Procedure: for_each_index
static void for_each_index(size_t N)
{

	tf::Executor executor{ 4 };
	tf::Taskflow taskflow;

	// [0, N) with step size 2
	taskflow.for_each_index(S_C(size_t, 0), N, S_C(size_t, 2), [](size_t) {
		// printf("for_each_index on index: %zu\n", i);
	});

	executor.run(taskflow).get();

	taskflow.dump(std::cout);
}

RECORD_INT(taskflow_parallel_for_test)
{
	HLVM_PROFILE_CPU_NAMED("taskflow_parallel_for_test");

	HLVM_LOG(LogTest, info, TXT("Test taskflow_parallel_for_test!"));
	{
		for_each(10000);
		for_each_index(10000);

		return 0;
	}
}

// https://github.com/taskflow/taskflow/blob/master/examples/parallel_sort.cpp

// generate a random string
static std::string random_string(size_t len)
{

	std::string		  tmp_s;
	static const char alphanum[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";

	std::default_random_engine		   eng{ std::random_device{}() };
	std::uniform_int_distribution<int> dist(1, 100000);

	tmp_s.reserve(len);

	for (size_t i = 0; i < len; ++i)
	{
		tmp_s += alphanum[S_C(size_t, dist(eng)) % (sizeof(alphanum) - 1)];
	}

	return tmp_s;
}

// generate a vector of random strings
static std::vector<std::string> random_strings()
{
	std::vector<std::string> strings(100000);
	std::cout << "generating random strings ...\n";
	// Task flow use 10 threads to gen random string
	tf::Executor executor;
	tf::Taskflow taskflow;
	taskflow.for_each(strings.begin(), strings.end(), [&](std::string& str) {
		str = random_string(32);
	});
	executor.run(taskflow).get();

	//	for (auto& str : strings)
	//	{
	//		str = random_string(32);
	//	}
	return strings;
}

RECORD_INT(taskflow_parallel_sort_test)
{
	HLVM_PROFILE_CPU_NAMED("taskflow_parallel_sort_test");

	HLVM_LOG(LogTest, info, TXT("Test taskflow_parallel_sort_test!"));
	{
		auto _strings = random_strings();
		// sequential sort
		{
			auto strings = CopyTemp(_strings);
			std::cout << "std::sort ... ";
			auto beg = std::chrono::steady_clock::now();
			std::sort(strings.begin(), strings.end());
			auto end = std::chrono::steady_clock::now();
			std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end - beg).count()
					  << " ms" << std::endl;
		}
		// parallel sort
		{
			auto strings = CopyTemp(_strings);
			std::cout << "Taskflow Parallel Sort ... ";
			auto beg = std::chrono::steady_clock::now();
			{
				tf::Taskflow taskflow;
				tf::Executor executor{ 4 };
				taskflow.sort(strings.begin(), strings.end());
				executor.run(taskflow).wait();
			}
			auto end = std::chrono::steady_clock::now();
			std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end - beg).count()
					  << " ms" << std::endl;
		}

		return 0;
	}
}

// https://github.com/taskflow/taskflow/blob/master/examples/priority.cpp
RECORD_INT(taskflow_priority_test)
{
	HLVM_PROFILE_CPU_NAMED("taskflow_priority_test");

	HLVM_LOG(LogTest, info, TXT("Test taskflow_priority_test!"));
	{
		// create an executor of only one worker to enable
		// deterministic behavior
		tf::Executor executor(1);

		tf::Taskflow taskflow;

		int counter{ 0 };

		// Here we create five tasks and print thier execution
		// orders which should align with assigned priorities
		auto [A, B, C, D, E] = taskflow.emplace(
			[]() {},
			[&]() {
				std::cout << "Task B: " << counter++ << '\n'; // 0
			},
			[&]() {
				std::cout << "Task C: " << counter++ << '\n'; // 2
			},
			[&]() {
				std::cout << "Task D: " << counter++ << '\n'; // 1
			},
			[]() {});

		A.precede(B, C, D);
		E.succeed(B, C, D);

		// By default, all tasks are of tf::TaskPriority::HIGH
		B.priority(tf::TaskPriority::HIGH);
		C.priority(tf::TaskPriority::LOW);
		D.priority(tf::TaskPriority::NORMAL);

		assert(B.priority() == tf::TaskPriority::HIGH);
		assert(C.priority() == tf::TaskPriority::LOW);
		assert(D.priority() == tf::TaskPriority::NORMAL);

		// we should see B, D, and C in their priority order
		executor.run(taskflow).wait();

		return 0;
	}
}

// https://github.com/taskflow/taskflow/blob/master/examples/reduce.cpp

#define MAX_DATA_SIZE 40000000
struct Data
{
	int a{ ::rand() };
	int b{ ::rand() };
	int transform() const
	{
		return a * a + 2 * a * b + b * b;
	}
};

// Procedure: reduce
// This procedure demonstrates
static void reduce()
{

	std::cout << "Benchmark: reduce" << std::endl;

	std::vector<int> data;
	data.reserve(MAX_DATA_SIZE);
	for (int i = 0; i < MAX_DATA_SIZE; ++i)
	{
		data.push_back(::rand());
	}

	// sequential method
	auto sbeg = std::chrono::steady_clock::now();
	auto smin = std::numeric_limits<int>::max();
	for (auto& d : data)
	{
		smin = std::min(smin, d);
	}
	auto send = std::chrono::steady_clock::now();
	std::cout << "[sequential] reduce: "
			  << std::chrono::duration_cast<std::chrono::microseconds>(send - sbeg).count()
			  << " us\n";

	// taskflow
	auto		 tbeg = std::chrono::steady_clock::now();
	tf::Taskflow taskflow;
	tf::Executor executor{ 4 };
	auto		 tmin = std::numeric_limits<int>::max();
	taskflow.reduce(
		data.begin(),
		data.end(),
		tmin,
		[](int& l, const auto& r) { return std::min(l, r); });
	executor.run(taskflow).get();
	auto tend = std::chrono::steady_clock::now();
	std::cout << "[taskflow] reduce: "
			  << std::chrono::duration_cast<std::chrono::microseconds>(tend - tbeg).count()
			  << " us\n";

	// assertion
	if (tmin == smin)
	{
		std::cout << "result is correct" << std::endl;
	}
	else
	{
		std::cout << "result is incorrect: " << smin << " != " << tmin << std::endl;
	}

	taskflow.dump(std::cout);
}

// Procedure: transform_reduce
static void transform_reduce()
{

	std::cout << "Benchmark: transform_reduce" << std::endl;

	std::vector<Data> data(MAX_DATA_SIZE);

	// sequential method
	auto sbeg = std::chrono::steady_clock::now();
	auto smin = std::numeric_limits<int>::max();
	for (auto& d : data)
	{
		smin = std::min(smin, d.transform());
	}
	auto send = std::chrono::steady_clock::now();
	std::cout << "[sequential] transform_reduce "
			  << std::chrono::duration_cast<std::chrono::microseconds>(send - sbeg).count()
			  << " us\n";

	// taskflow
	auto		 tbeg = std::chrono::steady_clock::now();
	tf::Taskflow tf;
	auto		 tmin = std::numeric_limits<int>::max();
	tf.transform_reduce(
		data.begin(), data.end(), tmin,
		[](int l, int r) { return std::min(l, r); },
		[](const Data& d) { return d.transform(); });
	tf::Executor().run(tf).get();
	auto tend = std::chrono::steady_clock::now();
	std::cout << "[taskflow] transform_reduce "
			  << std::chrono::duration_cast<std::chrono::microseconds>(tend - tbeg).count()
			  << " us\n";

	// assertion
	assert(tmin == smin);
}

RECORD_INT(taskflow_reduce_test)
{
	HLVM_PROFILE_CPU_NAMED("taskflow_reduce_test");

	HLVM_LOG(LogTest, info, TXT("Test taskflow_reduce_test!"));
	{

		{
			reduce();
		}
		{
			transform_reduce();
		}

		return 0;
	}
}

// https://github.com/taskflow/taskflow/blob/master/examples/runtime.cpp
RECORD_INT(taskflow_runtime_test)
{
	HLVM_PROFILE_CPU_NAMED("taskflow_runtime_test");

	HLVM_LOG(LogTest, info, TXT("Test taskflow_runtime_test!"));
	{
		tf::Taskflow taskflow("Runtime Tasking");
		tf::Executor executor{ 4 };

		tf::Task A, B, C, D;

		std::tie(A, B, C, D) = taskflow.emplace(
			[]() { std::cout << "A\n"; return 0; },
			[&C](tf::Runtime& rt) { // C must be captured by reference
				std::cout << "B\n";
				rt.schedule(C);
			},
			[]() { std::cout << "C\n"; },
			[]() { std::cout << "D\n"; });

		// name tasks
		A.name("A");
		B.name("B");
		C.name("C");
		D.name("D");

		// create conditional dependencies
		A.precede(B, C, D);

		// dump the graph structure
		taskflow.dump(std::cout);

		// we will see both B and C in the output
		executor.run(taskflow).wait();

		return 0;
	}
}

// https://github.com/taskflow/taskflow/blob/master/examples/simple.cpp
RECORD_INT(taskflow_simple_test)
{
	HLVM_PROFILE_CPU_NAMED("taskflow_simple_test");

	HLVM_LOG(LogTest, info, TXT("Test taskflow_simple_test!"));
	{
		tf::Executor executor{ 4 };
		tf::Taskflow taskflow("simple");

		auto [A, B, C, D] = taskflow.emplace(
			[]() { std::cout << "TaskA\n"; },
			[]() { std::cout << "TaskB\n"; },
			[]() { std::cout << "TaskC\n"; },
			[]() { std::cout << "TaskD\n"; });

		A.precede(B, C); // A runs before B and C
		D.succeed(B, C); // D runs after  B and C

		executor.run(taskflow).wait();

		return 0;
	}
}

// https://github.com/taskflow/taskflow/blob/master/examples/subflow.cpp
RECORD_INT(taskflow_subflow_test)
{
	HLVM_PROFILE_CPU_NAMED("taskflow_subflow_test");

	HLVM_LOG(LogTest, info, TXT("Test taskflow_subflow_test!"));
	{
		auto RunTest = [](bool detached) {
			// Create a taskflow graph with three regular tasks and one subflow task.
			tf::Executor executor(4);
			tf::Taskflow taskflow("Dynamic Tasking Demo");

			// Task A
			auto A = taskflow.emplace([]() { std::cout << "TaskA\n"; });
			auto B = taskflow.emplace(
				// Task B
				[cap = std::vector<int>{ 1, 2, 3, 4, 5, 6, 7, 8 }, detached](tf::Subflow& subflow) {
					std::cout << "TaskB is spawning B1, B2, and B3 ...\n";

					auto B1 = subflow.emplace([&]() {
										 printf("  Subtask B1: reduce sum = %d\n",
											 std::accumulate(cap.begin(), cap.end(), 0, std::plus<int>()));
									 })
								  .name("B1");

					auto B2 = subflow.emplace([&]() {
										 printf("  Subtask B2: reduce multiply = %d\n",
											 std::accumulate(cap.begin(), cap.end(), 1, std::multiplies<int>()));
									 })
								  .name("B2");

					auto B3 = subflow.emplace([&]() {
										 printf("  Subtask B3: reduce minus = %d\n",
											 std::accumulate(cap.begin(), cap.end(), 0, std::minus<int>()));
									 })
								  .name("B3");

					B1.precede(B3);
					B2.precede(B3);

					// detach or join the subflow (by default the subflow join at B)
					if (detached)
						subflow.detach();
				});

			auto C = taskflow.emplace([]() { std::cout << "TaskC\n"; });
			auto D = taskflow.emplace([]() { std::cout << "TaskD\n"; });
			A.name("A");
			B.name("B");
			C.name("C");
			D.name("D");

			A.precede(B); // B runs after A
			A.precede(C); // C runs after A
			B.precede(D); // D runs after B
			C.precede(D); // D runs after C

			executor.run(taskflow).get(); // block until finished

			// examine the graph
			taskflow.dump(std::cout);
		};

		RunTest(true);
		RunTest(false);

		return 0;
	}
}

// https://github.com/taskflow/taskflow/blob/master/examples/subflow_async.cpp
RECORD_INT(taskflow_subflow_async_test)
{
	HLVM_PROFILE_CPU_NAMED("taskflow_subflow_async_test");

	HLVM_LOG(LogTest, info, TXT("Test taskflow_subflow_async_test!"));
	{
		tf::Taskflow taskflow("Subflow Async");
		tf::Executor executor{ 4 };

		std::atomic<int> counter{ 0 };

		taskflow.emplace([&](tf::Subflow& sf) {
			for (int i = 0; i < 10; i++)
			{
				// Here, we use "silent_async" instead of "async" because we do
				// not care the return value. The method "silent_async" gives us
				// less overhead compared to "async".
				// The 10 asynchronous tasks run concurrently.
				sf.silent_async([&]() {
					std::cout << "async task from the subflow\n";
					counter.fetch_add(1, std::memory_order_relaxed);
				});
			}
			sf.join();
			std::cout << counter << " = 10\n";
		});

		executor.run(taskflow).wait();

		return 0;
	}
}

// https://github.com/taskflow/taskflow/blob/master/examples/visualization.cpp
RECORD_INT(taskflow_visualization_test)
{
	HLVM_PROFILE_CPU_NAMED("taskflow_visualization_test");

	HLVM_LOG(LogTest, info, TXT("Test taskflow_visualization_test!"));
	{
		tf::Taskflow taskflow("Visualization Demo");

		// ------------------------------------------------------
		// Static Tasking
		// ------------------------------------------------------
		auto A = taskflow.emplace([]() { std::cout << "Task A\n"; });
		auto B = taskflow.emplace([]() { std::cout << "Task B\n"; });
		auto C = taskflow.emplace([]() { std::cout << "Task C\n"; });
		auto D = taskflow.emplace([]() { std::cout << "Task D\n"; });
		auto E = taskflow.emplace([]() { std::cout << "Task E\n"; });

		A.precede(B, C, E);
		C.precede(D);
		B.precede(D, E);

		std::cout << "[dump without name assignment]\n";
		taskflow.dump(std::cout);

		std::cout << "[dump with name assignment]\n";
		A.name("A");
		B.name("B");
		C.name("C");
		D.name("D");
		E.name("E");

		// if the graph contains solely static tasks, you can simpley dump them
		// without running the graph
		taskflow.dump(std::cout);

		// ------------------------------------------------------
		// Dynamic Tasking
		// ------------------------------------------------------
		taskflow.emplace([](tf::Subflow& sf) {
			sf.emplace([]() { std::cout << "subflow task1"; }).name("s1");
			sf.emplace([]() { std::cout << "subflow task2"; }).name("s2");
			sf.emplace([]() { std::cout << "subflow task3"; }).name("s3");
		});

		// in order to visualize subflow tasks, you need to run the taskflow
		// to spawn the dynamic tasks first
		tf::Executor executor{ 4 };
		executor.run(taskflow).wait();

		taskflow.dump(std::cout);

		return 0;
	}
}

// https://github.com/taskflow/taskflow/blob/master/examples/while_loop.cpp
RECORD_INT(taskflow_while_loop_test)
{
	HLVM_PROFILE_CPU_NAMED("taskflow_while_loop_test");

	HLVM_LOG(LogTest, info, TXT("Test taskflow_while_loop_test!"));
	{
		tf::Executor executor{ 4 };
		tf::Taskflow taskflow;

		int i;

		auto [init, cond, body, back, done] = taskflow.emplace(
			[&]() { std::cout << "i=0\n"; i=0; },
			[&]() { std::cout << "while i<5\n"; return i < 5 ? 0 : 1; },
			[&]() { std::cout << "i++=" << i++ << '\n'; },
			[&]() { std::cout << "back\n"; return 0; },
			[&]() { std::cout << "done\n"; });

		init.name("init");
		cond.name("while i<5");
		body.name("i++");
		back.name("back");
		done.name("done");

		init.precede(cond);
		cond.precede(body, done);
		body.precede(back);
		back.precede(cond);

		taskflow.dump(std::cout);

		executor.run(taskflow).wait();

		return 0;
	}
}
