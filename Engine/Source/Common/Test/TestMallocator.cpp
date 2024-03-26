/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Test.h"

#include "Core/Mallocator/PMR.h"
#include "Core/Container/ContainerDefinition.h"
#include "Core/Parallel/ConcurrentQueue.h"

DECLARE_LOG_CATEGORY(LogTest)

RECORD(mallocator_test)
{
	{
		FMiMallocator MiMallocator{ { .bNewHeap = true } };
		HLVM_SCOPED_VARIABLE(
			ScopedMallocator, [&]() -> void { SwapMallocator(&MiMallocator); },
			[&]() -> void { SwapMallocator(); });

		// sample new and free
		{
			char* p1 = new char[100];
			char* p2 = new char[100];
			delete[] p1;
			delete[] p2;
		}
		{
			boost::container::vector<int, TMallocator<int>> vec{ TMallocator<int>(&MiMallocator) };
			vec.reserve(1000);
			for (size_t i = 0; i < 1000; i++)
			{
				vec.push_back(1);
			}
		}
	}
	{
		// Try use StackMallocator and you will have lifetime object crash on free
		TStackMallocator<16 * 1024> StackMallocator{};
		HLVM_SCOPED_VARIABLE(
			ScopedMallocator, [&]() -> void { SwapMallocator(&StackMallocator); },
			[&]() -> void { SwapMallocator(); });

		// sample new and free
		{
			char* p1 = new char[100];
			char* p2 = new char[100];
			delete[] p1;
			delete[] p2;
		}
		{
			boost::container::vector<int, TMallocator<int>> vec{ TMallocator<int>(&StackMallocator) };
			vec.reserve(1000);
			for (size_t i = 0; i < 1000; i++)
			{
				vec.push_back(1);
			}
		}
	}
}

RECORD(malloc_test)
{

	const size_t MAX_THREADS = 10;
	const size_t MAX_ITERATIONS = 10000;
	size_t		 MAX_BLOCK_SIZE = 1024 * 1024; // 1 MB

	auto allocate_and_deallocate = [&](size_t thread_id) {
		std::srand(1024);
		std::mt19937						  gen(1024);
		std::uniform_int_distribution<size_t> size_dist(1, MAX_BLOCK_SIZE);

		TBYTE*			   ptr = nullptr;
		std::queue<TBYTE*> free_list;
		for (size_t i = 0; i < MAX_ITERATIONS; ++i)
		{
			size_t size = size_dist(gen);
			ptr = new TBYTE[size];
			assert(ptr != nullptr);

			// Write data to the allocated memory
			memset(ptr, static_cast<int>(thread_id), size);

			// Read data from the allocated memory and verify
			for (size_t j = 0; j < size; ++j)
			{
				assert(reinterpret_cast<unsigned char*>(ptr)[j] == static_cast<unsigned char>(thread_id));
			}

			free_list.push(ptr);
			double random_number = S_C(double, std::rand()) / RAND_MAX;
			if (random_number < 0.5)
			{
				while (!free_list.empty())
				{
					ptr = free_list.front();
					delete[] (ptr);
					free_list.pop();
				}
			}
		}
		while (!free_list.empty())
		{
			ptr = free_list.front();
			delete[] (ptr);
			free_list.pop();
		}
	};

	auto test_single_thread = [&]() {
		FTimer timer;
		std::cout << "Running single-thread tests..." << std::endl;
		timer.Reset();
		allocate_and_deallocate(0);
		std::cout << "Single-thread tests passed! " << timer.Mark() << std::endl;
	};

	auto test_multi_thread = [&]() {
		std::cout << "Running multi-thread tests..." << std::endl;
		std::vector<std::thread> threads;

		for (size_t i = 0; i < MAX_THREADS; ++i)
		{
			threads.emplace_back(allocate_and_deallocate, i);
		}

		for (auto& thread : threads)
		{
			thread.join();
		}

		std::cout << "Multi-thread tests passed!" << std::endl;
	};

	auto test_different_block_sizes = [&]() {
		std::cout << "Running different block size tests..." << std::endl;
		std::vector<size_t> block_sizes = { 1, 8, 16, 32, 64, 128, 256, 512, 1024, 1024 * 1024 };

		for (size_t size : block_sizes)
		{
			TBYTE* ptr = new TBYTE[size];
			assert(ptr != nullptr);

			// Write data to the allocated memory
			memset(ptr, 0xAA, size);

			// Read data from the allocated memory and verify
			for (size_t i = 0; i < size; ++i)
			{
				assert(reinterpret_cast<unsigned char*>(ptr)[i] == 0xAA);
			}

			delete[] (ptr);
		}

		std::cout << "Different block size tests passed!" << std::endl;
	};

	{
		HLVM_LOG(LogTest, info, TXT("Test mimallocator"));
		FMiMallocator MiMallocator{ { .bNewHeap = true, .bDestory = true } };
		HLVM_SCOPED_VARIABLE(
			ScopedMallocator, [&]() -> void { SwapMallocator(&MiMallocator); MAX_BLOCK_SIZE = 1024; },
			[&]() -> void { SwapMallocator(); MAX_BLOCK_SIZE = 1024 * 1024; });
		test_single_thread();
	}

	{
		HLVM_LOG(LogTest, info, TXT("Test stack mallocator"));
		TStackMallocator<32 * 1024> StackMallocator{};
		HLVM_SCOPED_VARIABLE(
			ScopedMallocator, [&]() -> void { SwapMallocator(&StackMallocator); MAX_BLOCK_SIZE = 1024; },
			[&]() -> void { SwapMallocator(); MAX_BLOCK_SIZE = 1024 * 1024; });
		test_single_thread();
	}

	test_multi_thread();
	test_different_block_sizes();

	std::cout << "All tests passed!" << std::endl;
}
