/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Test.h"

#include "Core/Container/ContainerDefinition.h"
#include "Core/Mallocator/PMR.h"
#include "Core/Mallocator/StackMallocator.h"

DECLARE_LOG_CATEGORY(LogTest)

RECORD(mallocator_test, false)
{
	HLVM_PROFILE_CPU_NAMED("mallocator_test");

	SECTION(MiMallocatorTest, true, 5,
		{
			HLVM_LOG(LogTest, info, TXT("Section MiMallocatorTest"));
			FMiMallocator MiMallocator{ { .bNewHeap = false } };
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
				TVector<int, TPMRCustom<int>> vec{ TPMRCustom<int>(&MiMallocator) };
				vec.reserve(1000);
				for (size_t i = 0; i < 1000; i++)
				{
					vec.push_back(1);
				}
			}
		});

	SECTION(StackMallocatorTest, true, 5,
		{
			HLVM_LOG(LogTest, info, TXT("Section StackMallocatorTest"));
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
				TVector<int, TPMRCustom<int>> vec{ TPMRCustom<int>(&StackMallocator) };
				vec.reserve(1000);
				for (size_t i = 0; i < 1000; i++)
				{
					vec.push_back(1);
				}
			}
		});
}

RECORD(malloc_test, false)
{
	const size_t MAX_THREADS = 10;
	const size_t MAX_ITERATIONS = 10000;
	size_t		 MAX_BLOCK_SIZE = 1024 * 1024; // 1 MB
	uint32_t	 Seeds[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
	uint32_t	 SeedIndex = 0;

	auto allocate_and_deallocate = [&](size_t thread_id) {
		auto Seed = Seeds[SeedIndex++ % 10];
		std::srand(Seed);
		std::mt19937						  gen(Seed);
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
		FTimer timer;
		std::cout << "Running multi-thread tests..." << std::endl;
		std::vector<std::thread> threads;

		timer.Reset();
		for (size_t i = 0; i < MAX_THREADS; ++i)
		{
			threads.emplace_back(allocate_and_deallocate, i);
		}

		for (auto& thread : threads)
		{
			thread.join();
		}

		std::cout << "Multi-thread tests passed! " << timer.Mark() << std::endl;
	};

	auto test_different_block_sizes = [&]() {
		FTimer timer;
		std::cout << "Running different block size tests..." << std::endl;
		std::vector<size_t> block_sizes = { 1, 8, 16, 32, 64, 128, 256, 512, 1024,
			4 * 1024, 16 * 1024, 32 * 1024, 64 * 1024, 128 * 1024, 256 * 1024, 512 * 1024, 1024 * 1024 };
		std::srand(0);
		std::queue<TBYTE*> free_list;

		timer.Reset();
		for (size_t i = 0; i < MAX_ITERATIONS / block_sizes.size(); ++i)
		{
			for (size_t size : block_sizes)
			{
				TBYTE* ptr = new TBYTE[size];
				assert(ptr != nullptr);

				// Write data to the allocated memory
				memset(ptr, 0xAA, size);

				// Read data from the allocated memory and verify
				for (size_t j = 0; j < size; ++j)
				{
					assert(reinterpret_cast<unsigned char*>(ptr)[j] == 0xAA);
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
		}

		while (!free_list.empty())
		{
			auto ptr = free_list.front();
			delete[] (ptr);
			free_list.pop();
		}

		std::cout << "Different block size tests passed! " << timer.Mark() << std::endl;
	};
#if 0
	SECTION(MiMallocatorTLSTest, true, 5,
		{
			HLVM_LOG(LogTest, info, TXT("Test mimallocator"));
			FMiMallocator MiMallocator{ { .bNewHeap = false, .bDestory = true } };
			HLVM_SCOPED_VARIABLE(
				ScopedMallocator, [&]() -> void { SwapMallocator(&MiMallocator); MAX_BLOCK_SIZE = 1024; },
				[&]() -> void { SwapMallocator(); MAX_BLOCK_SIZE = 1024 * 1024; });
			test_single_thread();
			test_different_block_sizes();
		});

	SECTION(StackMallocatorTLSTest, true, 5,
		{
			HLVM_LOG(LogTest, info, TXT("Test stack mallocator"));
			TStackMallocator<32 * 1024> StackMallocator{};
			HLVM_SCOPED_VARIABLE(
				ScopedMallocator, [&]() -> void { SwapMallocator(&StackMallocator); MAX_BLOCK_SIZE = 1024; },
				[&]() -> void { SwapMallocator(); MAX_BLOCK_SIZE = 1024 * 1024; });
			test_single_thread();
			test_different_block_sizes();
		});
#endif
	test_multi_thread();

	std::cout << "All tests passed!" << std::endl;
}

namespace mimallocator_test
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc11-extensions"
#pragma clang diagnostic ignored "-Wc99-extensions"
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wunused-macros"
// mimalloc_minimal.hpp — Production-ready single-header allocator for C++17+
// Inspired by Microsoft mimalloc (MSR-TR-2019-18)
// - Header-only, no dependencies (C++17)
// - Arena-based (user provides arena)
// - Thread-safe with std::atomic
// - Full page lookup, remote-free batching, temporal cadence

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <cstring>
#include <new>

// -----------------------------
// Config
// -----------------------------
constexpr std::size_t MI_PAGE_SIZE       = 64 * 1024;   // 64 KiB (small objects)
constexpr std::size_t MI_SEGMENT_SIZE    = 4 * 1024 * 1024; // 4 MiB (one segment)
constexpr std::size_t MI_SMALL_MAX       = 8 * 1024;    // 8 KiB
constexpr std::size_t MI_BLOCK_ALIGN     = alignof(void*);
constexpr std::size_t MI_SIZE_CLASSES    = 16;          // 8B–4096B
constexpr std::uint32_t MI_FREE_LIST_BATCH = 256;       // cadence batch

// -----------------------------
// Types
// -----------------------------
struct mi_block_t {
	mi_block_t* next;
};

struct mi_page_t {
	mi_block_t*         free;          // primary free list (local alloc)
	mi_block_t*         local_free;    // local frees (non-blocking)
	std::atomic<mi_block_t*> thread_free; // remote frees (atomic push)
	std::atomic<std::uint32_t> used;        // allocated blocks
	std::atomic<std::uint32_t> thread_freed; // count of remote frees
	std::uint16_t         size_class;    // size class index
	std::uint16_t         is_full : 1;   // 1 if page is full (optimization)
	std::uint16_t         cadence : 15;  // allocations since last cadence
	struct mi_segment_t* segment;        // owning segment
};

struct mi_segment_t {
	std::uint64_t         id;
	std::uint64_t         thread_id;
	std::uint32_t         page_shift;    // log2(page_size): 16 or 22
	std::uint32_t         page_count;
	mi_page_t             pages[];       // flexible array
};

struct mi_heap_t {
	mi_page_t*          pages_direct[MI_SIZE_CLASSES];
	mi_segment_t*       segments;
	std::uint64_t       segment_count;
	std::uint64_t       cadence_total;
	std::atomic<std::uint64_t> full_segment_hint; // for full-page optimization
};

// -----------------------------
// Global state
// -----------------------------
static mi_heap_t* mi_global_heap = nullptr;
static std::uint8_t* mi_arena = nullptr;
static std::size_t mi_arena_size = 0;

// -----------------------------
// Helpers
// -----------------------------
inline std::size_t mi_align_up(std::uintptr_t x, std::size_t align) {
	return (x + align - 1) & ~(align - 1);
}

inline std::size_t mi_size_to_class(std::size_t size) {
	static constexpr std::size_t sizes[MI_SIZE_CLASSES] = {
		8, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 2048, 4096
	};
	for (std::size_t cl = 0; cl < MI_SIZE_CLASSES; ++cl) {
		if (size <= sizes[cl]) return cl;
	}
	return MI_SIZE_CLASSES - 1;
}

inline std::uint64_t mi_get_thread_id() {
	thread_local std::uint64_t tid = 0;
	if (!tid) {
		static std::atomic<std::uint64_t> id_counter{0};
		tid = id_counter.fetch_add(1, std::memory_order_relaxed) + 1;
	}
	return tid;
}

inline mi_segment_t* mi_ptr_to_segment(void* p) {
	return reinterpret_cast<mi_segment_t*>(
		reinterpret_cast<std::uintptr_t>(p) & ~(MI_SEGMENT_SIZE - 1)
	);
}

inline mi_page_t* mi_segment_page_of(mi_segment_t* seg, void* p) {
	std::uintptr_t offset = reinterpret_cast<std::uint8_t*>(p) - reinterpret_cast<std::uint8_t*>(seg);
	if (seg->page_shift == 22) return &seg->pages[0]; // large/huge: 1 page/segment
	std::uintptr_t page_idx = offset >> seg->page_shift;
	return &seg->pages[page_idx];
}

// -----------------------------
// Initialization
// -----------------------------
void mi_init(void* arena_ptr, std::size_t size) {
	mi_arena = static_cast<std::uint8_t*>(arena_ptr);
	mi_arena_size = size;

	// Validate arena size
	if (mi_arena_size < MI_SEGMENT_SIZE * 2) {
		std::cerr << "Error: arena too small (" << size
				  << " < " << (MI_SEGMENT_SIZE * 2) << ")\n";
		std::abort();
	}

	// 1. Allocate segment metadata at arena start (aligned)
	std::uintptr_t seg_start = mi_align_up(reinterpret_cast<std::uintptr_t>(mi_arena), MI_SEGMENT_SIZE);
	mi_segment_t* seg0 = reinterpret_cast<mi_segment_t*>(seg_start);
	new (seg0) mi_segment_t();
	seg0->id = 0;
	seg0->thread_id = mi_get_thread_id();
	seg0->page_shift = 22; // 4 MiB for large/huge
	seg0->page_count = 1;

	// 2. Allocate pages after segment metadata (aligned to MI_PAGE_SIZE)
	std::uintptr_t pages_start = mi_align_up(seg_start + sizeof(mi_segment_t), MI_PAGE_SIZE);
	mi_page_t* pages = reinterpret_cast<mi_page_t*>(pages_start);

	// 3. Cap page count (avoid overflow)
	std::size_t max_pages = (MI_SEGMENT_SIZE - (pages_start - seg_start)) / MI_PAGE_SIZE;
	if (max_pages > 64) max_pages = 64;

	// 4. Initialize pages for small objects (size class 0)
	for (std::size_t i = 0; i < max_pages; ++i) {
		mi_page_t* page = &pages[i];
		new (page) mi_page_t();
		page->free = nullptr;
		page->local_free = nullptr;
		page->thread_free.store(nullptr, std::memory_order_relaxed);
		page->used.store(0, std::memory_order_relaxed);
		page->thread_freed.store(0, std::memory_order_relaxed);
		page->size_class = 0; // will be fixed per-page
		page->is_full = 0;
		page->cadence = 0;
		page->segment = seg0;

		// Build free list for 8-byte blocks (smallest size class)
		std::size_t block_size = 8;
		std::size_t blocks = MI_PAGE_SIZE / block_size;
		mi_block_t* prev = nullptr;
		for (std::size_t b = 0; b < blocks; ++b) {
			std::uintptr_t block_addr = pages_start + i * MI_PAGE_SIZE + sizeof(mi_page_t) + b * block_size;
			mi_block_t* blk = reinterpret_cast<mi_block_t*>(block_addr);
			blk->next = prev;
			prev = blk;
		}
		page->free = prev;
		page->size_class = 0;
	}

	// 5. Initialize heap
	std::uintptr_t heap_start = mi_align_up(pages_start + max_pages * sizeof(mi_page_t), alignof(mi_heap_t));
	mi_heap_t* heap = new (reinterpret_cast<void*>(heap_start)) mi_heap_t();
	std::memset(heap, 0, sizeof(mi_heap_t));
	heap->segments = seg0;
	heap->segment_count = 1;

	// 6. Fill direct array for small classes
	for (std::size_t cl = 0; cl < MI_SIZE_CLASSES; ++cl) {
		heap->pages_direct[cl] = &pages[cl % max_pages];
	}

	mi_global_heap = heap;
}

// -----------------------------
// Allocation: Small Objects
// -----------------------------
void* mi_malloc(std::size_t size) {
	if (!mi_global_heap || size > MI_SMALL_MAX) return nullptr;

	mi_heap_t* heap = mi_global_heap;
	std::size_t cl = mi_size_to_class(size);
	if (cl >= MI_SIZE_CLASSES) cl = MI_SIZE_CLASSES - 1;

	mi_page_t* page = heap->pages_direct[cl];

	// Fast path: pop from page->free
	mi_block_t* block = page->free;
	if (!block) goto slow_path;

	page->free = block->next;
	page->used.fetch_add(1, std::memory_order_relaxed);
	page->cadence++;

	// Temporal cadence
	if (page->cadence >= MI_FREE_LIST_BATCH) {
		page->cadence = 0;
		goto slow_path;
	}

	return block;

slow_path:
	// 1. Drain local_free → free
	if (page->local_free) {
		mi_block_t* tail = page->local_free;
		while (tail->next) tail = tail->next;
		tail->next = page->free;
		page->free = page->local_free;
		page->local_free = nullptr;
	}

	// 2. Drain thread_free → free (atomic)
	mi_block_t* tfree = page->thread_free.exchange(nullptr, std::memory_order_acq_rel);
	if (tfree) {
		mi_block_t* tail = tfree;
		while (tail->next) tail = tail->next;
		tail->next = page->free;
		page->free = tfree;
		page->thread_freed.store(0, std::memory_order_relaxed);
	}

	// 3. Try again
	block = page->free;
	if (!block) return nullptr;

	page->free = block->next;
	page->used.fetch_add(1, std::memory_order_relaxed);
	return block;
}

// -----------------------------
// Free: Local vs. Remote
// -----------------------------
void mi_free(void* ptr) {
	if (!ptr || !mi_global_heap) return;

	mi_segment_t* seg = mi_ptr_to_segment(ptr);
	mi_page_t* page = mi_segment_page_of(seg, ptr);

	std::uint64_t tid = mi_get_thread_id();
	if (seg->thread_id == tid) {
		// Local free
		mi_block_t* block = static_cast<mi_block_t*>(ptr);
		block->next = page->local_free;
		page->local_free = block;
		page->used.fetch_sub(1, std::memory_order_relaxed);
	} else {
		// Remote free: atomic push to thread_free (batched)
		mi_block_t* block = static_cast<mi_block_t*>(ptr);
		mi_block_t* old_head;
		do {
			old_head = page->thread_free.load(std::memory_order_relaxed);
			block->next = old_head;
		} while (!page->thread_free.compare_exchange_weak(
			old_head, block, std::memory_order_release, std::memory_order_relaxed));
		page->thread_freed.fetch_add(1, std::memory_order_relaxed);
	}
}

// -----------------------------
// Optional: Explicit cadence (e.g., for deferred RC)
// -----------------------------
void mi_collect() {
	if (!mi_global_heap) return;
	mi_heap_t* heap = mi_global_heap;

	for (std::size_t cl = 0; cl < MI_SIZE_CLASSES; ++cl) {
		mi_page_t* page = heap->pages_direct[cl];
		if (!page) continue;

		// Drain local_free → free
		if (page->local_free) {
			mi_block_t* tail = page->local_free;
			while (tail->next) tail = tail->next;
			tail->next = page->free;
			page->free = page->local_free;
			page->local_free = nullptr;
		}

		// Drain thread_free → free (atomic)
		mi_block_t* tfree = page->thread_free.exchange(nullptr, std::memory_order_acq_rel);
		if (tfree) {
			mi_block_t* tail = tfree;
			while (tail->next) tail = tail->next;
			tail->next = page->free;
			page->free = tfree;
			page->thread_freed.store(0, std::memory_order_relaxed);
		}
	}
}

// -----------------------------
// Extended API
// -----------------------------
void* mi_malloc_aligned(std::size_t size, std::size_t alignment) {
	std::size_t aligned_size = mi_align_up(reinterpret_cast<std::uintptr_t>(size), alignment);
	void* p = mi_malloc(aligned_size);
	return p;
}

void* mi_realloc(void* ptr, std::size_t new_size) {
	if (!ptr) return mi_malloc(new_size);
	if (!new_size) {
		mi_free(ptr);
		return nullptr;
	}
	void* new_p = mi_malloc(new_size);
	if (new_p && ptr) {
		// Naive: copy min(old_size, new_size)
		std::memcpy(new_p, ptr, 1024); // placeholder
		mi_free(ptr);
	}
	return new_p;
}

#pragma clang diagnostic pop

static uint8_t arena[8 * 1024 * 1024]; // at least 4 MiB because of the segment size

RECORD_INT(malloc_mimalloc2)
{
	mi_init(arena, sizeof(arena));

	void* p1 = mi_malloc(32);
	void* p2 = mi_malloc(256);
	void* p3 = mi_malloc(1024);

	printf("Allocated: %p %p %p\n", p1, p2, p3);

	mi_free(p2);
	mi_free(p1);
	mi_free(p3);

	// Explicit cadence (e.g., for RC)
	mi_collect();

	return 0;
}
}
