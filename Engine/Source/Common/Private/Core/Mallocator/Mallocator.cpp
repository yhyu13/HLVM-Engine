/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Mallocator/MiMallocator.h"
#include "Core/Mallocator/StdMallocator.h"
#include "Core/Mallocator/StackMallocator.h"
#include "Core/Log.h"
#include "Template/PrintTemplate.tpp"
#include "Template/AlignmentTemplate.tpp"

/**
 * Override new and delete operator
 */
#ifndef HLVM_MALLOC_OVERRIDE
	#define HLVM_MALLOC_OVERRIDE 1
#endif

/**
 * Use stack allocator as general propose allocator
 * CAUTION : not recommanded, as long life time object e.g. share ptr counter, could lead to crash on free
 * turn off by default
 */
#ifndef HLVM_MALLOC_USE_GENERAL_PURPOSE_STACK_ALLOCATOR
	#define HLVM_MALLOC_USE_GENERAL_PURPOSE_STACK_ALLOCATOR 1
#endif

#ifndef HLVM_MALLOC_USE_CALLOC
	#define HLVM_MALLOC_USE_CALLOC HLVM_BUILD_DEBUG
#endif

#if HLVM_MALLOC_USE_CALLOC
	#define CALLOC(p, n) std::memset(p, 0, n)
#else
	#define CALLOC(p, n) ((void)0)
#endif

DECLARE_LOG_CATEGORY(LogMiMallocator)

/**
 * Mallocator time cost statistics
 */
HLVM_STATIC_VAR std::atomic<double> GMallocDurationCounter;
HLVM_STATIC_VAR std::atomic_uint_fast64_t GMallocCounter;
HLVM_STATIC_VAR std::atomic<double> GFreeDurationCounter;
HLVM_STATIC_VAR std::atomic_uint_fast64_t GFreeCounter;
#if !HLVM_BUILD_RELEASE
	#define TIME_MALLOC_CUM()                                   \
		GMallocCounter.fetch_add(1, std::memory_order_relaxed); \
		HLVM_SCOPED_TIMER_CUM_ATOMIC(GMallocDurationCounter, std::micro)
#else
	#define TIME_MALLOC_CUM() ((void)0)
#endif
#if !HLVM_BUILD_RELEASE
	#define TIME_FREE_CUM()                                   \
		GFreeCounter.fetch_add(1, std::memory_order_relaxed); \
		HLVM_SCOPED_TIMER_CUM_ATOMIC(GFreeDurationCounter, std::micro)
#else
	#define TIME_FREE_CUM() ((void)0)
#endif

void InitMallocator() // Extern
{
#if !HLVM_BUILD_RELEASE && HLVM_MALLOC_USE_MIMALLOC_OVER_STD
	mi_option_enable(mi_option_t::mi_option_show_errors);
	mi_option_enable(mi_option_t::mi_option_show_stats);
	mi_option_enable(mi_option_t::mi_option_verbose);
#endif
}

void FnalMallocator() // Extern
{
	HLVM_LOG(LogMiMallocator, trace, TXT("Mallocator finalize:\nCumulative time spent on malloc {} micro sec\nCumulative number of malloc {}\nPer malloc time {} micro sec\nCumulative time spent on free {} micro sec\nCumulative number of free {}\nPer free time {} micro sec"),
		GMallocDurationCounter.load(),
		GMallocCounter.load(),
		GMallocDurationCounter.load() / static_cast<double>(GMallocCounter.load()),
		GFreeDurationCounter.load(),
		GFreeCounter.load(),
		GFreeDurationCounter.load() / static_cast<double>(GFreeCounter.load()));
}

bool FMiMallocator::Owned(void* ptr) noexcept
{
	try
	{
		return mi_heap_check_owned(mHeap, ptr);
	}
	catch (std::exception& e)
	{
		HLVM_ENSURE(false, TO_TCHAR_STR(e.what()));
		return false;
	}
}

#if HLVM_MALLOC_OVERRIDE

HLVM_THREAD_LOCAL_VAR IMallocator* GMallocatorTLS = &GMiMallocatorTLS;			// Extern
HLVM_THREAD_LOCAL_VAR IMallocator* GFallBacllMallocatorTLS = &GMiMallocatorTLS; // Extern

void SwapMallocator(IMallocator* Mallocator) // Extern
{
	if (hlvm_private::GMallocatorTLSSwap == nullptr)
	{
		hlvm_private::GMallocatorTLSSwap = GMallocatorTLS;
		GMallocatorTLS = Mallocator;
	}
	else
	{
		GMallocatorTLS = hlvm_private::GMallocatorTLSSwap;
		hlvm_private::GMallocatorTLSSwap = nullptr;
	}
}

// Guide to override global new and delete : https://microsoft.github.io/mimalloc/using.html
// MIMALLOC_SHOW_STATS=1 ./Engine/Source/Common/Test/Test3rdParty
// <mimalloc-new-delete.h> is used as references to see what mimalloc has done in terms of overriding
// #include <mimalloc-new-delete.h>

// Below is the implementation of the new and delete operators copied from mimalloc
//*************************************************************************************************

	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wmissing-prototypes"
	// ----------------------------------------------------------------------------
	// This header provides convenient overrides for the new and
	// delete operations in C++.
	//
	// This header should be included in only one source file!
	//
	// On Windows, or when linking dynamically with mimalloc, these
	// can be more performant than the standard new-delete operations.
	// See <https://en.cppreference.com/w/cpp/memory/new/operator_new>
	// ---------------------------------------------------------------------------
	#if defined(__cplusplus)
		#include <new>
		#include <mimalloc.h>

		#if defined(_MSC_VER) && defined(_Ret_notnull_) && defined(_Post_writable_byte_size_)
		// stay consistent with VCRT definitions
			#define mi_decl_new(n) mi_decl_nodiscard mi_decl_restrict _Ret_notnull_ _Post_writable_byte_size_(n)
			#define mi_decl_new_nothrow(n) mi_decl_nodiscard mi_decl_restrict _Ret_maybenull_ _Success_(return != NULL) _Post_writable_byte_size_(n)
		#else
			#define mi_decl_new(n) mi_decl_nodiscard mi_decl_restrict
			#define mi_decl_new_nothrow(n) mi_decl_nodiscard mi_decl_restrict
		#endif

void operator delete(void* p) noexcept
{
	TIME_FREE_CUM();
	// StreamPrintf(&std::cout, "delete %s\n", R_C(uintptr_t, p));
	if (GMallocatorTLS->Free(p) != EFreeRetType::Success)
	{
		HLVM_ENSURE(GFallBacllMallocatorTLS->Free(p) == EFreeRetType::Success, TXT("delete failed {}"), p);
	}
};
void operator delete[](void* p) noexcept
{
	TIME_FREE_CUM();
	// StreamPrintf(&std::cout, "delete[] %s\n", R_C(uintptr_t, p));
	if (GMallocatorTLS->Free(p) != EFreeRetType::Success)
	{
		HLVM_ENSURE(GFallBacllMallocatorTLS->Free(p) == EFreeRetType::Success, TXT("delete failed {}"), p);
	}
};

void operator delete(void* p, const std::nothrow_t&) noexcept
{
	TIME_FREE_CUM();
	// StreamPrintf(&std::cout, "delete %s\n", R_C(uintptr_t, p));
	if (GMallocatorTLS->Free(p) != EFreeRetType::Success)
	{
		try
		{
			HLVM_ENSURE(GFallBacllMallocatorTLS->Free(p) == EFreeRetType::Success, TXT("delete failed {}"), p);
		}
		catch (...)
		{
		}
	}
}
void operator delete[](void* p, const std::nothrow_t&) noexcept
{
	TIME_FREE_CUM();
	// StreamPrintf(&std::cout, "delete[] %s\n", R_C(uintptr_t, p));
	if (GMallocatorTLS->Free(p) != EFreeRetType::Success)
	{
		try
		{
			HLVM_ENSURE(GFallBacllMallocatorTLS->Free(p) == EFreeRetType::Success, TXT("delete failed {}"), p);
		}
		catch (...)
		{
		}
	}
}

mi_decl_new(n) void* operator new(std::size_t n) noexcept(false)
{
	TIME_MALLOC_CUM();
	// StreamPrintf(&std::cout, "new %s\n", n);
	n = AlignUp(n, HLVM_MALLOC_ALIGNMENT);
	void* p = GMallocatorTLS->Malloc(n);
	if (!p)
	{
		p = GFallBacllMallocatorTLS->Malloc(n);
	}
	HLVM_ENSURE(p, TXT("new failed {}"), n);
	CALLOC(p, n);
	return p;
}
mi_decl_new(n) void* operator new[](std::size_t n) noexcept(false)
{
	TIME_MALLOC_CUM();
	// StreamPrintf(&std::cout, "new[] %s\n", n);
	n = AlignUp(n, HLVM_MALLOC_ALIGNMENT);
	void* p = GMallocatorTLS->Malloc(n);
	if (!p)
	{
		p = GFallBacllMallocatorTLS->Malloc(n);
	}
	HLVM_ENSURE(p, TXT("new failed {}"), n);
	CALLOC(p, n);
	return p;
}

mi_decl_new_nothrow(n) void* operator new(std::size_t n, const std::nothrow_t&) noexcept
{
	TIME_MALLOC_CUM();
	// StreamPrintf(&std::cout, "new %s\n", n);
	n = AlignUp(n, HLVM_MALLOC_ALIGNMENT);
	void* p = GMallocatorTLS->Malloc(n);
	if (!p)
	{
		p = GFallBacllMallocatorTLS->Malloc(n);
	}
	try
	{
		HLVM_ENSURE(p, TXT("new failed {}"), n);
		CALLOC(p, n);
	}
	catch (...)
	{
		p = nullptr;
	}
	return p;
}
mi_decl_new_nothrow(n) void* operator new[](std::size_t n, const std::nothrow_t&) noexcept
{
	TIME_MALLOC_CUM();
	// StreamPrintf(&std::cout, "new[] %s\n", n);
	n = AlignUp(n, HLVM_MALLOC_ALIGNMENT);
	void* p = GMallocatorTLS->Malloc2(n);
	if (!p)
	{
		p = GFallBacllMallocatorTLS->Malloc2(n);
	}
	try
	{
		HLVM_ENSURE(p, TXT("new failed {}"), n);
		CALLOC(p, n);
	}
	catch (...)
	{
		p = nullptr;
	}
	return p;
}

		#if (__cplusplus >= 201402L || _MSC_VER >= 1916)
void operator delete(void* p, std::size_t n) noexcept
{
	TIME_FREE_CUM();
	if (GMallocatorTLS->FreeSize(p, n) != EFreeRetType::Success)
	{
		HLVM_ENSURE(GFallBacllMallocatorTLS->FreeSize(p, n) == EFreeRetType::Success, TXT("delete failed {}"), p);
	}
};
void operator delete[](void* p, std::size_t n) noexcept
{
	TIME_FREE_CUM();
	if (GMallocatorTLS->FreeSize(p, n) != EFreeRetType::Success)
	{
		HLVM_ENSURE(GFallBacllMallocatorTLS->FreeSize(p, n) == EFreeRetType::Success, TXT("delete failed {}"), p);
	}
};
		#endif

		#if (__cplusplus > 201402L || defined(__cpp_aligned_new))
void operator delete(void* p, std::align_val_t al) noexcept
{
	TIME_FREE_CUM();
	size_t align = static_cast<size_t>(al);
	if (GMallocatorTLS->FreeAligned(p, align) != EFreeRetType::Success)
	{
		HLVM_ENSURE(GFallBacllMallocatorTLS->FreeAligned(p, align) == EFreeRetType::Success, TXT("delete failed {}"), p);
	}
}
void operator delete[](void* p, std::align_val_t al) noexcept
{
	TIME_FREE_CUM();
	size_t align = static_cast<size_t>(al);
	if (GMallocatorTLS->FreeAligned(p, align) != EFreeRetType::Success)
	{
		HLVM_ENSURE(GFallBacllMallocatorTLS->FreeAligned(p, align) == EFreeRetType::Success, TXT("delete failed {}"), p);
	}
}
void operator delete(void* p, std::size_t n, std::align_val_t al) noexcept
{
	TIME_FREE_CUM();
	size_t align = static_cast<size_t>(al);
	if (GMallocatorTLS->FreeSizeAligned(p, n, align) != EFreeRetType::Success)
	{
		HLVM_ENSURE(GFallBacllMallocatorTLS->FreeSizeAligned(p, n, align) == EFreeRetType::Success, TXT("delete failed {}"), p);
	}
};
void operator delete[](void* p, std::size_t n, std::align_val_t al) noexcept
{
	TIME_FREE_CUM();
	size_t align = static_cast<size_t>(al);
	if (GMallocatorTLS->FreeSizeAligned(p, n, align) != EFreeRetType::Success)
	{
		HLVM_ENSURE(GFallBacllMallocatorTLS->FreeSizeAligned(p, n, align) == EFreeRetType::Success, TXT("delete failed {}"), p);
	}
};
void operator delete(void* p, std::align_val_t al, const std::nothrow_t&) noexcept
{
	TIME_FREE_CUM();
	size_t align = static_cast<size_t>(al);
	if (GMallocatorTLS->FreeAligned(p, align) != EFreeRetType::Success)
	{
		try
		{
			HLVM_ENSURE(GFallBacllMallocatorTLS->FreeAligned(p, align) == EFreeRetType::Success, TXT("delete failed {}"), p);
		}
		catch (...)
		{
		}
	}
}
void operator delete[](void* p, std::align_val_t al, const std::nothrow_t&) noexcept
{
	TIME_FREE_CUM();
	size_t align = static_cast<size_t>(al);
	if (GMallocatorTLS->FreeAligned(p, align) != EFreeRetType::Success)
	{
		try
		{
			HLVM_ENSURE(GFallBacllMallocatorTLS->FreeAligned(p, align) == EFreeRetType::Success, TXT("delete failed {}"), p);
		}
		catch (...)
		{
		}
	}
}

void* operator new(std::size_t n, std::align_val_t al) noexcept(false)
{
	TIME_MALLOC_CUM();
	size_t align = static_cast<size_t>(al);
	n = AlignUp(n, align);
	void* p = GMallocatorTLS->MallocAligned(n, align);
	if (!p)
	{
		p = GFallBacllMallocatorTLS->MallocAligned(n, align);
	}
	HLVM_ENSURE(p, TXT("new failed {}"), n);
	CALLOC(p, n);
	return p;
}
void* operator new[](std::size_t n, std::align_val_t al) noexcept(false)
{
	TIME_MALLOC_CUM();
	size_t align = static_cast<size_t>(al);
	n = AlignUp(n, align);
	void* p = GMallocatorTLS->MallocAligned(n, align);
	if (!p)
	{
		p = GFallBacllMallocatorTLS->MallocAligned(n, align);
	}
	HLVM_ENSURE(p, TXT("new failed {}"), n);
	CALLOC(p, n);
	return p;
}
void* operator new(std::size_t n, std::align_val_t al, const std::nothrow_t&) noexcept
{
	TIME_MALLOC_CUM();
	size_t align = static_cast<size_t>(al);
	n = AlignUp(n, align);
	void* p = GMallocatorTLS->MallocAligned2(n, align);
	if (!p)
	{
		p = GFallBacllMallocatorTLS->MallocAligned2(n, align);
	}
	try
	{
		HLVM_ENSURE(p, TXT("new failed {}"), n);
		CALLOC(p, n);
	}
	catch (...)
	{
		p = nullptr;
	}
	return p;
}
void* operator new[](std::size_t n, std::align_val_t al, const std::nothrow_t&) noexcept
{
	TIME_MALLOC_CUM();
	size_t align = static_cast<size_t>(al);
	n = AlignUp(n, align);
	void* p = GMallocatorTLS->MallocAligned2(n, align);
	if (!p)
	{
		p = GFallBacllMallocatorTLS->MallocAligned2(n, align);
	}
	try
	{
		HLVM_ENSURE(p, TXT("new failed {}"), n);
		CALLOC(p, n);
	}
	catch (...)
	{
		p = nullptr;
	}
	return p;
}
		#endif

	#endif
	#pragma clang diagnostic pop

#else

HLVM_THREAD_LOCAL_VAR IMallocator* GMallocatorTLS = nullptr;		  // Extern
HLVM_THREAD_LOCAL_VAR IMallocator* GFallBacllMallocatorTLS = nullptr; // Extern
void							   SwapMallocator(IMallocator*)		  // Extern
{
	HLVM_NOT_IMPLEMENTED();
}

#endif
