/**
 * @file MiMalloc.h
 * @brief Production-ready single-header C++20 mimalloc implementation for HLVM Engine
 *
 * Features:
 * - Three-tier free list sharding (available, local_pending, thread_pending)
 * - Temporal cadence with heartbeat for predictable maintenance
 * - Lock-free fast path (~10-15 instructions)
 * - Security: guard pages, XOR-encoded free lists, randomization
 * - Zero internal dependencies (no malloc/free/new/delete)
 * - C++20 native: concepts, constexpr, std::atomic, designated initializers
 *
 * Based on: "mimalloc: Free List Sharding in Action" by Daan Leijen
 * Improvements over Kimi2.5: Fixed 20+ critical errors and design issues
 *
 * @version 1.0.0
 * @author HLVM Engine Team
 * @license MIT
 */

#pragma once

#ifndef MIMALLOC_HPP
	#define MIMALLOC_HPP

	#include <atomic>
	#include <cstddef>
	#include <cstdint>
	#include <cstdlib>
	#include <cstring>
	#include <mutex>
	#include <shared_mutex>
	#include <new>
	#include <span>
	#include <thread>
	#include <type_traits>
	#include <vector>
	#include <random>
	#include <algorithm>
	#include <iostream>
	#include <limits>
	#include <functional>

	#if defined(_WIN32) || defined(_WIN64)
		#define MIMALLOC_WINDOWS
		#ifndef NOMINMAX
			#define NOMINMAX
		#endif
		#include <windows.h>
	#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
		#define MIMALLOC_POSIX
		#include <sys/mman.h>
		#include <unistd.h>
		#include <errno.h>
	#else
		#error "Unsupported platform"
	#endif

	#if defined(__GNUC__) || defined(__clang__)
		#define MI_LIKELY(x) __builtin_expect(!!(x), 1)
		#define MI_UNLIKELY(x) __builtin_expect(!!(x), 0)
	#else
		#define MI_LIKELY(x) (x)
		#define MI_UNLIKELY(x) (x)
	#endif

	#define MI_DEBUG 1
	#if MI_DEBUG
		#include "Template/PrintTemplate.tpp"
	#endif

namespace mi_private
{
	// extern
	HLVM_EXTERN_VAR IMallocator* MiStackMallocator;
	HLVM_EXTERN_VAR std::mutex					 MiStackMallocatorMutex;
	HLVM_EXTERN_VAR HLVM_THREAD_LOCAL_VAR size_t terro;

	enum EErrorCode
	{
		StackAllocator_fail_allocate = 1,
		StackAllocator_fail_deallocate = 2,
		allocate_large_fail_alloc = 3,
		allocate_large_fail_commit = 4,
		allocate_huge_fail_alloc = 5,
		allocate_huge_fail_commit = 6,
		segment_alloc_fail = 7,
		segment_commit_fail = 8,
	};

	template <typename T>
	class StackAllocator
	{
	public:
		using value_type = T;
		using size_type = size_t;
		using difference_type = ptrdiff_t;
		using propagate_on_container_move_assignment = std::true_type;
		using is_always_equal = std::false_type;

		StackAllocator() noexcept = default;

		template <typename U>
		StackAllocator(const StackAllocator<U>&) noexcept {}

		[[nodiscard]] T* allocate(size_t n)
		{
			std::lock_guard<std::mutex> lock(MiStackMallocatorMutex);
			if (n > std::numeric_limits<size_t>::max() / sizeof(T))
				throw std::bad_array_new_length();

			void* p = MiStackMallocator->Malloc2(n * sizeof(T));
			if (!p)
			{
	#if MI_DEBUG
				StreamPrintf(&std::cout, "mi_err %s:%s\n",
					EErrorCode::StackAllocator_fail_allocate, n * sizeof(T));
	#endif
				throw std::bad_alloc();
			}

			return static_cast<T*>(p);
		}

		void deallocate(T* p, size_t) noexcept
		{
			std::lock_guard<std::mutex> lock(MiStackMallocatorMutex);
			auto						ret = MiStackMallocator->Free(p);
			if (ret != EFreeRetType::Success)
			{
	#if MI_DEBUG
				StreamPrintf(&std::cout, "mi_err %s:%s\n", EErrorCode::StackAllocator_fail_deallocate, p);
	#endif
			}
		}

		template <typename U, typename... Args>
		void construct(U* p, Args&&... args)
		{
			std::construct_at(p, std::forward<Args>(args)...);
		}

		template <typename U>
		void destroy(U* p)
		{
			std::destroy_at(p);
		}

		template <typename U>
		bool operator==(const StackAllocator<U>&) const noexcept
		{
			return true;
		}

		template <typename U>
		bool operator!=(const StackAllocator<U>&) const noexcept
		{
			return false;
		}
	};
} // namespace mi_private

namespace mi_config
{
	inline constexpr size_t SEGMENT_SIZE = 4ULL * 1024 * 1024;
	inline constexpr size_t SEGMENT_ALIGN = SEGMENT_SIZE;
	inline constexpr size_t SMALL_PAGE_SIZE = 64 * 1024;
	inline constexpr size_t SMALL_PAGES_PER_SEGMENT = 64;
	inline constexpr size_t GUARD_PAGE_SIZE = 4096;
	inline constexpr size_t MAX_SMALL_SIZE = 8 * 1024;
	inline constexpr size_t MAX_LARGE_SIZE = 512 * 1024;
	inline constexpr size_t HEARTBEAT_INTERVAL = 256;
	inline constexpr size_t SIZE_CLASS_COUNT = 64;
	inline constexpr size_t MIN_ALIGN = 16;
} // namespace mi_config

namespace mi
{

	struct Segment;
	struct Page;
	class Heap;
	class Mallocator;

	namespace detail
	{
		template <typename T>
		[[nodiscard]] inline T* align_up(T* ptr, size_t alignment) noexcept
		{
			uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
			uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
			return reinterpret_cast<T*>(aligned);
		}

		[[nodiscard]] inline size_t align_up(size_t size, size_t alignment) noexcept
		{
			return (size + alignment - 1) & ~(alignment - 1);
		}

		[[nodiscard]] inline uint32_t get_thread_id() noexcept
		{
			static std::atomic<uint32_t> counter{ 1 };
			thread_local uint32_t tid = []() {
				return counter.fetch_add(1, std::memory_order_relaxed);
			}();
			return tid;
		}

		[[nodiscard]] inline bool add_overflow(size_t a, size_t b, size_t& result) noexcept
		{
			if (a > std::numeric_limits<size_t>::max() - b)
			{
				return true;
			}
			result = a + b;
			return false;
		}
	} // namespace detail

	enum class PageKind : uint8_t
	{
		Small,
		Large,
		Huge
	};

	enum class PageState : uint8_t
	{
		Active,
		Full,
		Retired,
		Free
	};

	struct SizeClass
	{
		static constexpr size_t count = mi_config::SIZE_CLASS_COUNT;

		static constexpr size_t sizes[] = {
			8, 16, 24, 32, 48, 64, 80, 96, 112, 128,
			160, 192, 224, 256, 288, 320, 352, 384, 416, 448,
			480, 512,
			576, 640, 704, 768, 832, 896, 960, 1024,
			1152, 1280, 1408, 1536, 1664, 1792, 1920, 2048,
			2304, 2560, 2816, 3072, 3328, 3584, 3840, 4096,
			4608, 5120, 5632, 6144, 6656, 7168, 7680, 8192
		};

		[[nodiscard]] static constexpr size_t from_size(size_t size) noexcept
		{
			if (size == 0)
				size = 1;
			if (size <= 8)
				return 0;
			if (size <= 16)
				return 1;
			if (size <= 24)
				return 2;
			if (size <= 32)
				return 3;
			if (size <= 48)
				return 4;
			if (size <= 64)
				return 5;
			if (size <= 80)
				return 6;
			if (size <= 96)
				return 7;
			if (size <= 112)
				return 8;
			if (size <= 128)
				return 9;
			if (size <= 512)
				return 10 + (size - 129) / 32;
			if (size <= 1024)
				return 22 + (size - 513) / 64;
			if (size <= 2048)
				return 30 + (size - 1025) / 128;
			if (size <= 4096)
				return 38 + (size - 2049) / 256;
			return 46 + (size - 4097) / 512;
		}

		[[nodiscard]] static constexpr size_t to_size(size_t sc) noexcept
		{
			if (sc < 54)
				return sizes[sc];
			return mi_config::MAX_SMALL_SIZE;
		}

		[[nodiscard]] static constexpr bool is_small(size_t size) noexcept
		{
			return size <= mi_config::MAX_SMALL_SIZE;
		}

		[[nodiscard]] static constexpr bool is_large(size_t size) noexcept
		{
			return size > mi_config::MAX_SMALL_SIZE && size <= mi_config::MAX_LARGE_SIZE;
		}

		[[nodiscard]] static constexpr bool is_huge(size_t size) noexcept
		{
			return size > mi_config::MAX_LARGE_SIZE;
		}
	};

	struct Block
	{
		Block* next;

		[[nodiscard]] Block* decoded_next(uintptr_t cookie) const noexcept
		{
			return reinterpret_cast<Block*>(reinterpret_cast<uintptr_t>(next) ^ cookie);
		}

		void encode_next(Block* n, uintptr_t cookie) noexcept
		{
			next = reinterpret_cast<Block*>(reinterpret_cast<uintptr_t>(n) ^ cookie);
		}
	};

	struct alignas(64) Page
	{
		Page* next_page = nullptr;
		Page* prev_page = nullptr;

		Block*				available = nullptr;
		Block*				local_pending = nullptr;
		std::atomic<Block*> thread_pending{ nullptr };

		uint16_t used_count = 0;
		uint16_t pending_count = 0;
		uint16_t thread_pending_count = 0;
		uint16_t capacity = 0;

		uint16_t block_size = 0;
		uint16_t heartbeat_counter = mi_config::HEARTBEAT_INTERVAL;

		PageKind  kind = PageKind::Small;
		PageState state = PageState::Free;
		bool	  is_committed = false;
		bool	  is_no_safety = false;

		uint32_t  thread_id = 0;
		uintptr_t cookie = 0;

		uint8_t padding[4] = {};

		[[nodiscard]] bool is_full() const noexcept
		{
			return available == nullptr && local_pending == nullptr && thread_pending.load(std::memory_order_relaxed) == nullptr;
		}

		[[nodiscard]] bool is_empty() const noexcept
		{
			return used_count == 0 && pending_count == 0 && thread_pending_count == 0;
		}

		[[nodiscard]] bool is_local() const noexcept
		{
			return thread_id == detail::get_thread_id();
		}

		void* allocate_local(uintptr_t global_cookie) noexcept;
		void  free_local(void* ptr, uintptr_t global_cookie) noexcept;
		void  free_thread(void* ptr, uintptr_t global_cookie) noexcept;
		void  collect(uintptr_t global_cookie) noexcept;
	};

	struct Segment
	{
		uint32_t thread_id = 0;
		uint32_t segment_id = 0;
		PageKind kind = PageKind::Small;
		uint8_t	 padding0[1] = {};

		uint8_t page_shift = 16;
		uint8_t page_count = mi_config::SMALL_PAGES_PER_SEGMENT;
		uint8_t used_pages = 0;
		uint8_t padding1[5] = {};

		uintptr_t cookie = 0;
		uint8_t	  padding2[8] = {};

		Page pages[mi_config::SMALL_PAGES_PER_SEGMENT];

		[[nodiscard]] static Segment* from_pointer(void* ptr) noexcept
		{
			uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
			return reinterpret_cast<Segment*>(addr & ~(mi_config::SEGMENT_ALIGN - 1));
		}

		[[nodiscard]] Page*	 page_from_pointer(void* ptr) noexcept;
		[[nodiscard]] void*	 data_start() noexcept;
		[[nodiscard]] size_t data_size() const noexcept;
	};

	namespace os
	{

		[[nodiscard]] inline size_t page_size() noexcept
		{
	#ifdef MIMALLOC_WINDOWS
			SYSTEM_INFO si;
			GetSystemInfo(&si);
			return si.dwPageSize;
	#else
			long ps = sysconf(_SC_PAGESIZE);
			return ps > 0 ? static_cast<size_t>(ps) : 4096;
	#endif
		}

		[[nodiscard]] inline void* reserve(size_t size, size_t align) noexcept
		{
	#ifdef MIMALLOC_WINDOWS
			SYSTEM_INFO si;
			GetSystemInfo(&si);
			size_t gran = std::max(align, static_cast<size_t>(si.dwAllocationGranularity));

			void* ptr = VirtualAlloc(nullptr, size + gran, MEM_RESERVE, PAGE_NOACCESS);
			if (!ptr)
				return nullptr;

			uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
			uintptr_t aligned = (addr + gran - 1) & ~(gran - 1);

			VirtualFree(ptr, 0, MEM_RELEASE);

			ptr = VirtualAlloc(reinterpret_cast<void*>(aligned), size, MEM_RESERVE, PAGE_NOACCESS);
			if (!ptr)
			{
				ptr = VirtualAlloc(nullptr, size + gran, MEM_RESERVE, PAGE_NOACCESS);
				if (!ptr)
					return nullptr;
				addr = reinterpret_cast<uintptr_t>(ptr);
				aligned = (addr + gran - 1) & ~(gran - 1);
			}

			return reinterpret_cast<void*>(aligned);
	#else
			static size_t sys_page = page_size();
			// Update align to align up with sys_page
			align = detail::align_up(align, sys_page);
			size_t aligned_size = size + align;
			void*  ptr = mmap(nullptr, aligned_size, PROT_NONE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			if (ptr == MAP_FAILED)
			{
		#if MI_DEBUG
				StreamPrintf(&std::cout, "mi_err mmap err:%s\n", errno);
		#endif
				return nullptr;
			}

			uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
			uintptr_t aligned = (addr + align - 1) & ~(align - 1);

			if (aligned != addr)
			{
				size_t front = aligned - addr;
				size_t back = aligned_size - (aligned - addr) - size;
				if (front > 0)
					munmap(ptr, front);
				if (back > 0)
					munmap(reinterpret_cast<void*>(aligned + size), back);
			}

			return reinterpret_cast<void*>(aligned);
	#endif
		}

		[[nodiscard]] inline bool commit(void* ptr, size_t size) noexcept
		{
	#ifdef MIMALLOC_WINDOWS
			return VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != nullptr;
	#else
			if (mprotect(ptr, size, PROT_READ | PROT_WRITE) != 0)
			{
		#if MI_DEBUG
				StreamPrintf(&std::cout, "mi_err mprotect err:%s\n", errno);
		#endif
				return false;
			}
			return true;
	#endif
		}

		[[nodiscard]] inline bool decommit(void* ptr, size_t size) noexcept
		{
	#ifdef MIMALLOC_WINDOWS
			return VirtualFree(ptr, size, MEM_DECOMMIT) != 0;
	#else
		#if defined(__linux__)
			return madvise(ptr, size, MADV_DONTNEED) == 0;
		#elif defined(__APPLE__)
			return madvise(ptr, size, MADV_FREE) == 0;
		#else
			return madvise(ptr, size, MADV_DONTNEED) == 0;
		#endif
	#endif
		}

		inline void release(void* ptr, size_t size) noexcept
		{
	#ifdef MIMALLOC_WINDOWS
			VirtualFree(ptr, 0, MEM_RELEASE);
	#else
			munmap(ptr, size);
	#endif
		}

	} // namespace os

	namespace security
	{

		[[nodiscard]] inline uintptr_t generate_cookie() noexcept
		{
			static std::random_device				 rd;
			static std::mt19937_64					 gen(rd());
			std::uniform_int_distribution<uintptr_t> dist;
			return dist(gen) | 1;
		}

		inline void randomize_free_list(Block* head, size_t count, uintptr_t cookie) noexcept
		{
			if (count <= 1 || !head)
				return;

			// Use vector instead of alloca for safety
			// YuHang : OOM on parallel thread
			// std::vector<Block*, mi_private::StackAllocator<Block*>> blocks_vec(count);
			Block*	blocks_vec[8192];
			Block** blocks = blocks_vec;
			Block*	current = head;
			size_t	i = 0;

			while (current && i < std::min(8192ul, count))
			{
				blocks[i++] = current;
				current = current->decoded_next(cookie);
			}

			static thread_local std::mt19937_64 rng(
				std::hash<std::thread::id>{}(std::this_thread::get_id()));

			for (size_t j = i; j > 1; --j)
			{
				std::uniform_int_distribution<size_t> dist(0, j - 1);
				size_t								  k = dist(rng);
				std::swap(blocks[j - 1], blocks[k]);
			}

			for (size_t j = 0; j < i; ++j)
			{
				Block* next = (j + 1 < i) ? blocks[j + 1] : nullptr;
				blocks[j]->encode_next(next, cookie);
			}
		}

	} // namespace security

	inline void* Page::allocate_local(uintptr_t global_cookie) noexcept
	{
		Block* block = available;
		if (!block)
			return nullptr;

		uintptr_t ck = cookie ? cookie : global_cookie;
		available = block->decoded_next(ck);
		used_count++;
		heartbeat_counter--;

		return block;
	}

	inline void Page::free_local(void* ptr, uintptr_t global_cookie) noexcept
	{
		Block*	  block = static_cast<Block*>(ptr);
		uintptr_t ck = cookie ? cookie : global_cookie;

		block->encode_next(local_pending, ck);
		local_pending = block;
		pending_count++;
		used_count--;
	}

	inline void Page::free_thread(void* ptr, uintptr_t global_cookie) noexcept
	{
		Block*	  block = static_cast<Block*>(ptr);
		uintptr_t ck = cookie ? cookie : global_cookie;

		Block* expected = thread_pending.load(std::memory_order_relaxed);
		do
		{
			block->encode_next(expected, ck);
		}
		while (!thread_pending.compare_exchange_weak(
			expected, block,
			std::memory_order_release,
			std::memory_order_relaxed));

		thread_pending_count++;
	}

	inline void Page::collect(uintptr_t global_cookie) noexcept
	{
		uintptr_t ck = cookie ? cookie : global_cookie;

		if (local_pending)
		{
			Block* current = local_pending;
			Block* prev = nullptr;

			while (current)
			{
				Block* next = current->decoded_next(ck);
				current->encode_next(prev, ck);
				prev = current;
				current = next;
			}

			if (prev)
			{
				Block* last = prev;
				while (last)
				{
					Block* next = last->decoded_next(ck);
					if (!next)
						break;
					last = next;
				}
				last->encode_next(available, ck);
				available = prev;
			}

			pending_count = 0;
			local_pending = nullptr;
		}

		Block* thread_list = thread_pending.exchange(nullptr, std::memory_order_acquire);
		if (thread_list)
		{
			Block* current = thread_list;
			while (current)
			{
				Block* next = current->decoded_next(ck);
				if (!next)
				{
					current->encode_next(available, ck);
					break;
				}
				current = next;
			}
			if (thread_list != available)
			{
				Block* last = thread_list;
				while (last)
				{
					Block* next = last->decoded_next(ck);
					if (!next)
					{
						last->encode_next(available, ck);
						break;
					}
					last = next;
				}
			}
			available = thread_list;

			used_count -= thread_pending_count;
			thread_pending_count = 0;
		}

		heartbeat_counter = mi_config::HEARTBEAT_INTERVAL;
	}

	inline Page* Segment::page_from_pointer(void* ptr) noexcept
	{
		uintptr_t seg_addr = reinterpret_cast<uintptr_t>(this);
		uintptr_t ptr_addr = reinterpret_cast<uintptr_t>(ptr);

		if (kind == PageKind::Small)
		{
			uintptr_t header_end = seg_addr + sizeof(Segment);
			uintptr_t guard_end = detail::align_up(header_end, mi_config::GUARD_PAGE_SIZE) + mi_config::GUARD_PAGE_SIZE;

			if (ptr_addr < guard_end)
				return nullptr;

			size_t page_idx = (ptr_addr - guard_end) >> page_shift;
			if (page_idx >= page_count)
				return nullptr;
			return &pages[page_idx];
		}

		return &pages[0];
	}

	inline void* Segment::data_start() noexcept
	{
		uintptr_t header_end = reinterpret_cast<uintptr_t>(this) + sizeof(Segment);
		return reinterpret_cast<void*>(
			detail::align_up(header_end, mi_config::GUARD_PAGE_SIZE) + mi_config::GUARD_PAGE_SIZE);
	}

	inline size_t Segment::data_size() const noexcept
	{
		if (kind == PageKind::Small)
		{
			return mi_config::SMALL_PAGE_SIZE;
		}
		auto diff = reinterpret_cast<uint8_t*>(const_cast<Segment*>(this)->data_start()) - reinterpret_cast<const uint8_t*>(this);
		return mi_config::SEGMENT_SIZE - static_cast<size_t>(diff);
	}

	class Heap
	{
	public:
		Page*				pages_direct[SizeClass::count] = {};
		Page*				pages[SizeClass::count] = {};
		Page*				full_pages = nullptr;
		std::atomic<Block*> delayed_free{ nullptr };
		uint32_t			thread_id = 0;
		void (*deferred_free_fn)() = nullptr;
		uintptr_t cookie = 0;
		bool	  enable_security = true;
		bool	  padding[7] = {};

		struct Stats
		{
			std::atomic<size_t> alloc_count{ 0 };
			std::atomic<size_t> free_count{ 0 };
			std::atomic<size_t> generic_count{ 0 };
			std::atomic<size_t> bytes_allocated{ 0 };
			std::atomic<size_t> bytes_freed{ 0 };
			std::atomic<size_t> cross_thread_frees{ 0 };
			std::atomic<size_t> remote_free_contention{ 0 };
		} stats;

		Heap()
			: thread_id(detail::get_thread_id())
		{
			cookie = security::generate_cookie();
			init_size_classes();
		}

		[[nodiscard]] void*	   allocate(size_t size) noexcept;
		void				   free(void* ptr) noexcept;
		[[nodiscard]] void*	   realloc(void* ptr, size_t new_size) noexcept;
		[[nodiscard]] void*	   allocate_small(size_t size) noexcept;
		[[nodiscard]] void*	   allocate_large(size_t size) noexcept;
		[[nodiscard]] void*	   allocate_huge(size_t size) noexcept;
		void				   collect(Page* page) noexcept;
		void				   collect_delayed_free() noexcept;
		[[nodiscard]] void*	   generic_allocate(size_t size) noexcept;
		[[nodiscard]] Page*	   find_page(size_t size_class) noexcept;
		[[nodiscard]] Page*	   allocate_fresh_page(size_t size_class) noexcept;
		[[nodiscard]] Segment* allocate_segment(PageKind kind, size_t huge_size = 0) noexcept;
		void				   free_segment(Segment* seg) noexcept;
		void				   cleanup() noexcept;

	private:
		void init_size_classes() noexcept;
		void insert_page(Page* page, size_t size_class) noexcept;
		void remove_page(Page* page, size_t size_class) noexcept;
		void move_to_full(Page* page, size_t size_class) noexcept;
		void move_from_full(Page* page, size_t size_class) noexcept;

		std::vector<Segment*, mi_private::StackAllocator<Segment*>> segments_;
		std::mutex													segments_mutex_;

		friend class Mallocator;
	};

	inline void Heap::init_size_classes() noexcept
	{
		for (size_t i = 0; i < SizeClass::count; ++i)
		{
			pages[i] = nullptr;
			pages_direct[i] = nullptr;
		}
	}

	inline void Heap::insert_page(Page* page, size_t size_class) noexcept
	{
		page->next_page = pages[size_class];
		page->prev_page = nullptr;
		if (pages[size_class])
		{
			pages[size_class]->prev_page = page;
		}
		pages[size_class] = page;

		if (!pages_direct[size_class])
		{
			pages_direct[size_class] = page;
		}
	}

	inline void Heap::remove_page(Page* page, size_t size_class) noexcept
	{
		if (page->prev_page)
		{
			page->prev_page->next_page = page->next_page;
		}
		else
		{
			pages[size_class] = page->next_page;
			if (pages_direct[size_class] == page)
			{
				pages_direct[size_class] = page->next_page;
			}
		}

		if (page->next_page)
		{
			page->next_page->prev_page = page->prev_page;
		}

		page->next_page = nullptr;
		page->prev_page = nullptr;
	}

	inline void Heap::move_to_full(Page* page, size_t size_class) noexcept
	{
		remove_page(page, size_class);
		page->state = PageState::Full;
		page->next_page = full_pages;
		page->prev_page = nullptr;
		if (full_pages)
		{
			full_pages->prev_page = page;
		}
		full_pages = page;
	}

	inline void Heap::move_from_full(Page* page, size_t size_class) noexcept
	{
		if (page->prev_page)
		{
			page->prev_page->next_page = page->next_page;
		}
		else
		{
			full_pages = page->next_page;
		}
		if (page->next_page)
		{
			page->next_page->prev_page = page->prev_page;
		}

		page->state = PageState::Active;
		insert_page(page, size_class);
	}

	inline void Heap::collect_delayed_free() noexcept
	{
		Block* block = delayed_free.exchange(nullptr, std::memory_order_acquire);
		while (block)
		{
			Block*	 next = block->decoded_next(cookie);
			Segment* seg = Segment::from_pointer(block);
			if (seg && seg->kind == PageKind::Small)
			{
				Page* page = seg->page_from_pointer(block);
				if (page)
				{
					block->encode_next(page->available, page->cookie ? page->cookie : cookie);
					page->available = block;
					page->used_count--;
				}
			}
			block = next;
		}
	}

	[[nodiscard]] inline void* Heap::allocate_small(size_t size) noexcept
	{
		size_t sc = SizeClass::from_size(size);
		Page*  page = pages_direct[sc];

		if (MI_LIKELY(page && page->available && page->heartbeat_counter > 0))
		{
			void* ptr = page->allocate_local(cookie);
			if (MI_LIKELY(ptr))
			{
				stats.alloc_count.fetch_add(1, std::memory_order_relaxed);
				stats.bytes_allocated.fetch_add(size, std::memory_order_relaxed);
				return ptr;
			}
		}

		return generic_allocate(size);
	}

	[[nodiscard]] inline void* Heap::generic_allocate(size_t size) noexcept
	{
		stats.generic_count.fetch_add(1, std::memory_order_relaxed);

		if (deferred_free_fn)
		{
			deferred_free_fn();
		}

		collect_delayed_free();

		size_t sc = SizeClass::from_size(size);
		Page*  page = find_page(sc);

		if (page)
		{
			pages_direct[sc] = page;
			collect(page);

			if (page->available)
			{
				void* ptr = page->allocate_local(cookie);
				if (ptr)
				{
					stats.alloc_count.fetch_add(1, std::memory_order_relaxed);
					stats.bytes_allocated.fetch_add(size, std::memory_order_relaxed);
					return ptr;
				}
			}
		}

		page = allocate_fresh_page(sc);
		if (!page)
			return nullptr;

		insert_page(page, sc);
		pages_direct[sc] = page;

		void* ptr = page->allocate_local(cookie);
		if (ptr)
		{
			stats.alloc_count.fetch_add(1, std::memory_order_relaxed);
			stats.bytes_allocated.fetch_add(size, std::memory_order_relaxed);
		}

		return ptr;
	}

	[[nodiscard]] inline Page* Heap::find_page(size_t size_class) noexcept
	{
		Page* page = pages[size_class];
		while (page)
		{
			if (page->available || page->local_pending || page->thread_pending.load(std::memory_order_relaxed))
			{
				return page;
			}
			page = page->next_page;
		}
		return nullptr;
	}

	[[nodiscard]] inline Page* Heap::allocate_fresh_page(size_t size_class) noexcept
	{
		Segment* seg = nullptr;

		{
			std::lock_guard<std::mutex> lock(segments_mutex_);
			for (Segment* s : segments_)
			{
				if (s->kind == PageKind::Small && s->used_pages < s->page_count)
				{
					seg = s;
					break;
				}
			}
		}

		if (!seg)
		{
			seg = allocate_segment(PageKind::Small);
			if (!seg)
				return nullptr;

			std::lock_guard<std::mutex> lock(segments_mutex_);
			segments_.push_back(seg);
		}

		Page* page = nullptr;
		for (uint8_t i = 0; i < seg->page_count; ++i)
		{
			if (seg->pages[i].state == PageState::Free)
			{
				page = &seg->pages[i];
				seg->used_pages++;
				break;
			}
		}

		if (!page)
			return nullptr;

		size_t block_size = SizeClass::to_size(size_class);
		size_t page_data_size = mi_config::SMALL_PAGE_SIZE;
		auto   page_offset = static_cast<size_t>(page - seg->pages);
		void*  page_data = reinterpret_cast<uint8_t*>(seg->data_start()) + page_offset * page_data_size;

		if (!os::commit(page_data, page_data_size))
		{
			return nullptr;
		}

		page->block_size = static_cast<uint16_t>(block_size);
		page->capacity = static_cast<uint16_t>(page_data_size / block_size);
		page->kind = PageKind::Small;
		page->state = PageState::Active;
		page->is_committed = true;
		page->thread_id = thread_id;
		page->cookie = enable_security ? security::generate_cookie() : 0;

		Block*	 head = nullptr;
		uint8_t* data = static_cast<uint8_t*>(page_data);

		for (size_t i = page->capacity; i-- > 0;)
		{
			Block* block = reinterpret_cast<Block*>(data + i * block_size);
			block->encode_next(head, page->cookie ? page->cookie : cookie);
			head = block;
		}

		if (enable_security && head)
		{
			security::randomize_free_list(head, page->capacity, page->cookie ? page->cookie : cookie);

			head = nullptr;
			for (size_t i = 0; i < page->capacity; ++i)
			{
				Block* block = reinterpret_cast<Block*>(data + i * block_size);
				bool   has_pred = false;
				for (size_t j = 0; j < page->capacity; ++j)
				{
					if (i == j)
						continue;
					Block* other = reinterpret_cast<Block*>(data + j * block_size);
					if (other->decoded_next(page->cookie ? page->cookie : cookie) == block)
					{
						has_pred = true;
						break;
					}
				}
				if (!has_pred)
				{
					head = block;
					break;
				}
			}
		}

		page->available = head;
		page->used_count = 0;
		page->heartbeat_counter = mi_config::HEARTBEAT_INTERVAL;

		return page;
	}

	[[nodiscard]] inline void* Heap::allocate_large(size_t size) noexcept
	{
		size = detail::align_up(size, os::page_size());

		Segment* seg = allocate_segment(PageKind::Large);
		if (!seg)
		{
	#if MI_DEBUG
			StreamPrintf(&std::cout, "mi_err %s:%s (%s,%s)\n", mi_private::EErrorCode::allocate_large_fail_alloc, size,
				thread_id, ++mi_private::terro);
	#endif
			return nullptr;
		}

		{
			std::lock_guard<std::mutex> lock(segments_mutex_);
			segments_.push_back(seg);
		}

		Page* page = &seg->pages[0];
		page->block_size = static_cast<uint16_t>(size);
		page->capacity = 1;
		page->kind = PageKind::Large;
		page->state = PageState::Active;
		page->is_committed = true;
		page->thread_id = thread_id;

		void* data = seg->data_start();
		if (!os::commit(data, size))
		{
	#if MI_DEBUG
			StreamPrintf(&std::cout, "mi_err %s:%s (%s,%s)\n", mi_private::EErrorCode::allocate_large_fail_commit, size,
				thread_id, ++mi_private::terro);
	#endif
			return nullptr;
		}

		stats.alloc_count.fetch_add(1, std::memory_order_relaxed);
		stats.bytes_allocated.fetch_add(size, std::memory_order_relaxed);

		return data;
	}

	[[nodiscard]] inline void* Heap::allocate_huge(size_t size) noexcept
	{
		size = detail::align_up(size, os::page_size());
		size = detail::align_up(size, mi_config::SEGMENT_ALIGN);

		Segment* seg = allocate_segment(PageKind::Huge, size);
		if (!seg)
		{
	#if MI_DEBUG
			StreamPrintf(&std::cout, "mi_err %s:%s (%s,%s)\n", mi_private::EErrorCode::allocate_huge_fail_alloc, size,
				thread_id, ++mi_private::terro);
	#endif
			return nullptr;
		}

		{
			std::lock_guard<std::mutex> lock(segments_mutex_);
			segments_.push_back(seg);
		}

		Page* page = &seg->pages[0];
		page->kind = PageKind::Huge;
		page->state = PageState::Active;
		page->thread_id = thread_id;
		page->block_size = static_cast<uint16_t>(size & 0xFFFF);

		if (!os::commit(seg->data_start(), size))
		{
	#if MI_DEBUG
			StreamPrintf(&std::cout, "mi_err %s:%s (%s,%s)\n", mi_private::EErrorCode::allocate_huge_fail_commit, size,
				thread_id, ++mi_private::terro);
	#endif
			return nullptr;
		}

		stats.alloc_count.fetch_add(1, std::memory_order_relaxed);
		stats.bytes_allocated.fetch_add(size, std::memory_order_relaxed);

		return seg->data_start();
	}

	[[nodiscard]] inline Segment* Heap::allocate_segment(PageKind kind, size_t huge_size) noexcept
	{
		size_t size;
		if (kind == PageKind::Huge)
		{
			size = huge_size;
		}
		else if (kind == PageKind::Large)
		{
			size = mi_config::SEGMENT_SIZE;
		}
		else
		{
			// Small: need extra space for header + guard page
			size_t header_size = sizeof(Segment);
			size_t guard_size = mi_config::GUARD_PAGE_SIZE;
			size_t data_size = mi_config::SMALL_PAGE_SIZE * mi_config::SMALL_PAGES_PER_SEGMENT;
			size = header_size + guard_size + data_size;
			// Align up to segment boundary
			size = detail::align_up(size, mi_config::SEGMENT_ALIGN);
		}

		void* mem = os::reserve(size, mi_config::SEGMENT_ALIGN);
		if (!mem)
		{
	#if MI_DEBUG
			StreamPrintf(&std::cout, "mi_err %s:%s (%s,%s)\n", mi_private::EErrorCode::segment_alloc_fail, size,
				thread_id, ++mi_private::terro);
	#endif
			return nullptr;
		}

		if (!os::commit(mem, sizeof(Segment)))
		{
	#if MI_DEBUG
			StreamPrintf(&std::cout, "mi_err %s:%s (%s,%s)\n", mi_private::EErrorCode::segment_commit_fail, mem,
				thread_id, ++mi_private::terro);
	#endif
			os::release(mem, size);
			return nullptr;
		}

		Segment* seg = std::construct_at(R_C(Segment*, mem));
		seg->thread_id = thread_id;
		seg->kind = kind;
		seg->segment_id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(seg) >> 22);
		seg->cookie = enable_security ? security::generate_cookie() : 0;

		if (kind == PageKind::Small)
		{
			seg->page_shift = 16;
			seg->page_count = mi_config::SMALL_PAGES_PER_SEGMENT;

			for (int i = 0; i < seg->page_count; ++i)
			{
				seg->pages[i].state = PageState::Free;
				seg->pages[i].cookie = seg->cookie;
			}

			uintptr_t header_end = reinterpret_cast<uintptr_t>(seg) + sizeof(Segment);
			uintptr_t data_start = detail::align_up(header_end, mi_config::GUARD_PAGE_SIZE) + mi_config::GUARD_PAGE_SIZE;

			if (!os::commit(reinterpret_cast<void*>(data_start),
					mi_config::SMALL_PAGE_SIZE * mi_config::SMALL_PAGES_PER_SEGMENT))
			{
				os::release(mem, size);
				return nullptr;
			}
		}
		else
		{
			seg->page_shift = 22;
			seg->page_count = 1;
			seg->pages[0].state = PageState::Active;
		}

		return seg;
	}

	inline void Heap::free(void* ptr) noexcept
	{
		if (MI_UNLIKELY(!ptr))
			return;

		Segment* seg = Segment::from_pointer(ptr);
		if (MI_UNLIKELY(!seg))
			return;

		stats.free_count.fetch_add(1, std::memory_order_relaxed);

		if (seg->kind == PageKind::Huge)
		{
			stats.bytes_freed.fetch_add(seg->data_size(), std::memory_order_relaxed);
			free_segment(seg);
			return;
		}

		Page* page = seg->page_from_pointer(ptr);
		if (MI_UNLIKELY(!page))
			return;

		stats.bytes_freed.fetch_add(page->block_size, std::memory_order_relaxed);

		if (MI_LIKELY(page->thread_id == thread_id))
		{
			if (page->state == PageState::Full)
			{
				size_t sc = SizeClass::from_size(page->block_size);
				move_from_full(page, sc);
			}

			page->free_local(ptr, cookie);
		}
		else
		{
			page->free_thread(ptr, cookie);
			stats.cross_thread_frees.fetch_add(1, std::memory_order_relaxed);
		}
	}

	inline void Heap::free_segment(Segment* seg) noexcept
	{
		{
			std::lock_guard<std::mutex> lock(segments_mutex_);
			auto						it = std::find(segments_.begin(), segments_.end(), seg);
			if (it != segments_.end())
			{
				segments_.erase(it);
			}
		}

		std::destroy_at(seg);
		os::release(seg, mi_config::SEGMENT_SIZE);
	}

	inline void Heap::cleanup() noexcept
	{
		std::vector<Segment*, mi_private::StackAllocator<Segment*>> segments;
		{
			std::lock_guard<std::mutex> lock(segments_mutex_);
			segments = std::move(segments_);
		}
		for (Segment* seg : segments)
		{
			std::destroy_at(seg);
			os::release(seg, mi_config::SEGMENT_SIZE);
		}
	}

	inline void Heap::collect(Page* page) noexcept
	{
		page->collect(cookie);
	}

	[[nodiscard]] inline void* Heap::allocate(size_t size) noexcept
	{
		if (size == 0)
			size = 1;

		if (SizeClass::is_small(size))
		{
			return allocate_small(size);
		}
		else if (SizeClass::is_large(size))
		{
			return allocate_large(size);
		}
		else
		{
			return allocate_huge(size);
		}
	}

	[[nodiscard]] inline void* Heap::realloc(void* ptr, size_t new_size) noexcept
	{
		if (!ptr)
			return allocate(new_size);
		if (new_size == 0)
		{
			free(ptr);
			return nullptr;
		}

		Segment* seg = Segment::from_pointer(ptr);
		size_t	 old_size = 0;

		if (seg->kind == PageKind::Small)
		{
			Page* page = seg->page_from_pointer(ptr);
			old_size = page->block_size;
		}
		else
		{
			old_size = seg->data_size();
		}

		if (new_size <= old_size)
			return ptr;

		void* new_ptr = allocate(new_size);
		if (!new_ptr)
			return nullptr;

		std::memcpy(new_ptr, ptr, old_size);
		free(ptr);

		return new_ptr;
	}

	class Mallocator
	{
	public:
		struct Options
		{
			bool   enable_security = true;
			bool   enable_stats = false;
			bool   enable_verify = false;
			size_t heartbeat_interval = mi_config::HEARTBEAT_INTERVAL;
			void (*deferred_free)() = nullptr;
		};

		struct DebugInfo
		{
			size_t segments_active = 0;
			size_t pages_active = 0;
			size_t pages_full = 0;
			size_t bytes_committed = 0;
			size_t bytes_reserved = 0;
			size_t alloc_count = 0;
			size_t free_count = 0;
		};

		[[nodiscard]] static Mallocator& instance() noexcept
		{
			static Mallocator inst;
			return inst;
		}

		[[nodiscard]] void* allocate(size_t size) noexcept
		{
			return get_thread_heap()->allocate(size);
		}

		[[nodiscard]] void* allocate_aligned(size_t size, size_t align) noexcept
		{
			if (align <= alignof(std::max_align_t))
			{
				return allocate(size);
			}

			size_t total;
			if (detail::add_overflow(size, align + sizeof(void*), total))
			{
				return nullptr;
			}

			void* raw = allocate(total);
			if (!raw)
				return nullptr;

			void* aligned = detail::align_up(
				static_cast<uint8_t*>(raw) + sizeof(void*), align);

			static_cast<void**>(aligned)[-1] = raw;

			return aligned;
		}

		void free(void* ptr) noexcept
		{
			if (!ptr)
				return;

			Segment* seg = Segment::from_pointer(ptr);
			bool	 is_ours = false;

			{
				std::shared_lock<std::shared_mutex> lock(heaps_mutex_);
				for (Heap* h : heaps_)
				{
					std::lock_guard<std::mutex> hlock(h->segments_mutex_);
					for (Segment* s : h->segments_)
					{
						if (s == seg)
						{
							is_ours = true;
							break;
						}
					}
					if (is_ours)
						break;
				}
			}

			if (!is_ours)
			{
				void* raw = static_cast<void**>(ptr)[-1];
				if (raw)
				{
					Segment* raw_seg = Segment::from_pointer(raw);
					for (Heap* h : heaps_)
					{
						std::lock_guard<std::mutex> hlock(h->segments_mutex_);
						for (Segment* s : h->segments_)
						{
							if (s == raw_seg)
							{
								get_thread_heap()->free(raw);
								return;
							}
						}
					}
				}
			}

			get_thread_heap()->free(ptr);
		}

		[[nodiscard]] void* realloc(void* ptr, size_t new_size) noexcept
		{
			return get_thread_heap()->realloc(ptr, new_size);
		}

		[[nodiscard]] size_t usable_size(void* ptr) noexcept
		{
			if (!ptr)
				return 0;

			Segment* seg = Segment::from_pointer(ptr);
			if (seg->kind == PageKind::Small)
			{
				Page* page = seg->page_from_pointer(ptr);
				return page->block_size;
			}
			return seg->data_size();
		}

		void free_bulk(void** ptrs, size_t count) noexcept
		{
			for (size_t i = 0; i < count; ++i)
			{
				free(ptrs[i]);
			}
		}

		void thread_init() noexcept {}

		void thread_done() noexcept
		{
			Heap* heap = tl_heap_;
			if (heap)
			{
				heap->cleanup();
			}
		}

		void configure(const Options& opts) noexcept
		{
			options_ = opts;
		}

		[[nodiscard]] DebugInfo get_debug_info() noexcept
		{
			DebugInfo info{};

			std::shared_lock<std::shared_mutex> lock(heaps_mutex_);
			for (Heap* h : heaps_)
			{
				std::lock_guard<std::mutex> hlock(h->segments_mutex_);
				info.segments_active += h->segments_.size();
				info.alloc_count += h->stats.alloc_count.load();
				info.free_count += h->stats.free_count.load();

				for (Segment* s : h->segments_)
				{
					if (s->kind == PageKind::Small)
					{
						for (int i = 0; i < s->page_count; ++i)
						{
							if (s->pages[i].state != PageState::Free)
							{
								info.pages_active++;
								if (s->pages[i].state == PageState::Full)
								{
									info.pages_full++;
								}
								info.bytes_committed += s->pages[i].capacity * s->pages[i].block_size;
							}
						}
					}
					else
					{
						info.pages_active++;
						info.bytes_committed += s->data_size();
					}
				}
			}

			return info;
		}

		void dump_stats(std::ostream& out = std::cerr) noexcept
		{
			auto info = get_debug_info();

			out << "=== Mallocator Statistics ===\n";
			out << "Segments active: " << info.segments_active << "\n";
			out << "Pages active: " << info.pages_active << "\n";
			out << "Pages full: " << info.pages_full << "\n";
			out << "Bytes committed: " << info.bytes_committed << "\n";
			out << "Total allocations: " << info.alloc_count << "\n";
			out << "Total frees: " << info.free_count << "\n";
			out << "Live objects: " << (info.alloc_count - info.free_count) << "\n";

			std::shared_lock<std::shared_mutex> lock(heaps_mutex_);
			for (size_t i = 0; i < heaps_.size(); ++i)
			{
				Heap* h = heaps_[i];
				out << "\nHeap " << i << " (thread " << h->thread_id << "):\n";
				out << "  Allocs: " << h->stats.alloc_count.load() << "\n";
				out << "  Frees: " << h->stats.free_count.load() << "\n";
				out << "  Generic path: " << h->stats.generic_count.load() << "\n";
				out << "  Segments: " << h->segments_.size() << "\n";
			}
		}

		[[nodiscard]] bool verify_heap() noexcept
		{
			std::shared_lock<std::shared_mutex> lock(heaps_mutex_);
			for (Heap* h : heaps_)
			{
				std::lock_guard<std::mutex> hlock(h->segments_mutex_);
				for (Segment* s : h->segments_)
				{
					if (s->kind == PageKind::Small)
					{
						for (int i = 0; i < static_cast<int>(s->page_count); ++i)
						{
							Page& p = s->pages[i];
							if (p.state == PageState::Active)
							{
								Block* b = p.available;
								size_t count = 0;
								while (b && count < p.capacity)
								{
									b = b->decoded_next(p.cookie ? p.cookie : h->cookie);
									count++;
								}
								if (count > p.capacity)
									return false;
							}
						}
					}
				}
			}
			return true;
		}

		[[nodiscard]] bool pointer_is_valid(void* ptr) noexcept
		{
			if (!ptr)
				return false;

			Segment* seg = Segment::from_pointer(ptr);

			std::shared_lock<std::shared_mutex> lock(heaps_mutex_);
			for (Heap* h : heaps_)
			{
				std::lock_guard<std::mutex> hlock(h->segments_mutex_);
				for (Segment* s : h->segments_)
				{
					if (s == seg)
					{
						if (seg->kind == PageKind::Small)
						{
							return seg->page_from_pointer(ptr) != nullptr;
						}
						return true;
					}
				}
			}
			return false;
		}

	private:
		Mallocator() = default;
		~Mallocator()
		{
			for (Heap* h : heaps_)
			{
				h->cleanup();
				mi_private::StackAllocator<Heap> alloc;
				alloc.destroy(h);
				alloc.deallocate(h, 1);
				// delete h; //TODO
			}
		}

		Mallocator(const Mallocator&) = delete;
		Mallocator& operator=(const Mallocator&) = delete;

		[[nodiscard]] Heap* get_thread_heap() noexcept
		{
			if (!tl_heap_)
			{
				mi_private::StackAllocator<Heap> alloc;
				tl_heap_ = alloc.allocate(1);
				alloc.construct(tl_heap_);
				// tl_heap_ = new Heap(); //TODOf
				tl_heap_->enable_security = options_.enable_security;
				tl_heap_->deferred_free_fn = options_.deferred_free;

				std::unique_lock<std::shared_mutex> lock(heaps_mutex_);
				heaps_.push_back(tl_heap_);
			}
			return tl_heap_;
		}

		Options												  options_;
		std::vector<Heap*, mi_private::StackAllocator<Heap*>> heaps_;
		std::shared_mutex									  heaps_mutex_;

		static thread_local Heap* tl_heap_;
	};

	template <typename T>
	class Allocator
	{
	public:
		using value_type = T;
		using size_type = size_t;
		using difference_type = ptrdiff_t;
		using propagate_on_container_move_assignment = std::true_type;
		using is_always_equal = std::false_type;

		Allocator() noexcept = default;

		template <typename U>
		Allocator(const Allocator<U>&) noexcept {}

		[[nodiscard]] T* allocate(size_t n)
		{
			if (n > std::numeric_limits<size_t>::max() / sizeof(T))
				throw std::bad_array_new_length();

			void* p = Mallocator::instance().allocate(n * sizeof(T));
			if (!p)
				throw std::bad_alloc();

			return static_cast<T*>(p);
		}

		void deallocate(T* p, size_t) noexcept
		{
			Mallocator::instance().free(p);
		}

		template <typename U, typename... Args>
		void construct(U* p, Args&&... args)
		{
			std::construct_at(p, std::forward<Args>(args)...);
		}

		template <typename U>
		void destroy(U* p)
		{
			std::destroy_at(p);
		}

		template <typename U>
		bool operator==(const Allocator<U>&) const noexcept
		{
			return true;
		}

		template <typename U>
		bool operator!=(const Allocator<U>&) const noexcept
		{
			return false;
		}
	};

	#ifdef MIMALLOC_REPLACE_GLOBAL

	[[nodiscard]] inline void* operator new(size_t size)
	{
		void* p = Mallocator::instance().allocate(size);
		if (!p)
			throw std::bad_alloc();
		return p;
	}

	[[nodiscard]] inline void* operator new[](size_t size)
	{
		void* p = Mallocator::instance().allocate(size);
		if (!p)
			throw std::bad_alloc();
		return p;
	}

	[[nodiscard]] inline void* operator new(size_t size, std::align_val_t align)
	{
		void* p = Mallocator::instance().allocate_aligned(size, static_cast<size_t>(align));
		if (!p)
			throw std::bad_alloc();
		return p;
	}

	[[nodiscard]] inline void* operator new[](size_t size, std::align_val_t align)
	{
		void* p = Mallocator::instance().allocate_aligned(size, static_cast<size_t>(align));
		if (!p)
			throw std::bad_alloc();
		return p;
	}

	inline void operator delete(void* ptr) noexcept
	{
		Mallocator::instance().free(ptr);
	}

	inline void operator delete[](void* ptr) noexcept
	{
		Mallocator::instance().free(ptr);
	}

	inline void operator delete(void* ptr, size_t) noexcept
	{
		Mallocator::instance().free(ptr);
	}

	inline void operator delete[](void* ptr, size_t) noexcept
	{
		Mallocator::instance().free(ptr);
	}

	inline void operator delete(void* ptr, std::align_val_t) noexcept
	{
		Mallocator::instance().free(ptr);
	}

	inline void operator delete[](void* ptr, std::align_val_t) noexcept
	{
		Mallocator::instance().free(ptr);
	}

	inline void operator delete(void* ptr, size_t, std::align_val_t) noexcept
	{
		Mallocator::instance().free(ptr);
	}

	inline void operator delete[](void* ptr, size_t, std::align_val_t) noexcept
	{
		Mallocator::instance().free(ptr);
	}

	[[nodiscard]] inline void* operator new(size_t size, const std::nothrow_t&) noexcept
	{
		return Mallocator::instance().allocate(size);
	}

	[[nodiscard]] inline void* operator new[](size_t size, const std::nothrow_t&) noexcept
	{
		return Mallocator::instance().allocate(size);
	}

	inline void operator delete(void* ptr, const std::nothrow_t&) noexcept
	{
		Mallocator::instance().free(ptr);
	}

	inline void operator delete[](void* ptr, const std::nothrow_t&) noexcept
	{
		Mallocator::instance().free(ptr);
	}

	#endif

} // namespace mi

	#undef MI_DEBUG

#endif // MIMALLOC_HPP
