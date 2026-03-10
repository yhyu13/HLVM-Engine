/**
* Copyright (c) 2025. MIT License. All rights reserved.
*/

#include "Core/Mallocator/Mi/MiMalloc.h"
#include "Core/Mallocator/StackMallocator.h"

namespace mi
{
	thread_local Heap* Mallocator::tl_heap_ = nullptr;
}

namespace mi_private
{
	/**
	 * Assert stack mallocator, adjust reserved memory size to your needs (default 512Kb)
	 * mi stack allocator mainly for vector of seg* or heap*, a 512Kb pool equals maximum 64k pointers
	 * and 64k seg* if each is for large segment (65536 bytes=64kb) is about 4GB allocated memory
	 * if you see "mi_err 1:XXX" followed by std::bad_alloc in the error log, try double the size
	 */
	HLVM_STATIC_VAR TStackMallocator<512 * 1024, false, true, false, false, true> stack_mallocator{};
	// extern
	IMallocator* MiStackMallocator{ &stack_mallocator };
	// extern
	std::mutex MiStackMallocatorMutex{};
	// extern
	HLVM_THREAD_LOCAL_VAR size_t terro = 0;
}
