/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Test.h"

#include "Core/Mallocator/StackMallocator.h"
#include "Core/Container/ContainerDefinition.h"

DELCARE_LOG_CATEGORY(LogTest)

RECORD(mallocator_test)
{
	{
		FMiMallocator MiMallocator{ { .bNewHeap = true } };
		HLVM_SCOPED_VARIABLE(
			ScopedMallocator, [&]() -> void { SwapMallocator(&MiMallocator); },
			[&]() -> void { SwapMallocator(); });

		// sample new and free
		{
			char* p1 = new char(100);
			char* p2 = new char(100);
			delete p1;
			delete p2;
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
		FStackMallocator<4 * 1024> StackMallocator{ {
			.bMonolithic = false,
		} };
		HLVM_SCOPED_VARIABLE(
			ScopedMallocator, [&]() -> void { SwapMallocator(&StackMallocator); },
			[&]() -> void { SwapMallocator(); });

		// sample new and free
		{
			char* p1 = new char(100);
			char* p2 = new char(100);
			delete p1;
			delete p2;
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
