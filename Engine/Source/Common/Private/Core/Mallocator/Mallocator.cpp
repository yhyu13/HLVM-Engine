/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Core/Mallocator/MiMallocator.h"
#include "Core/Mallocator/StdMallocator.h"
#include "Core/Mallocator/StackMallocator.h"
#include "Core/Mallocator/VMMallocator/VMMallocator.h"
#include "Core/Log.h"
#include "Core/Delegate.h"
#include "Template/PrintTemplate.tpp"
#include "Template/AlignmentTemplate.tpp"

#include "Utility/Profiler/ProfilerCPU.h"
#include "Utility/ScopedTimer.h"

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
#if !HLVM_SHIPPING
	#define _PROFILE_MALLOC_CUMULATIVE()                         \
		GMallocCounter.fetch_add(1, std::memory_order_relaxed); \
		HLVM_SCOPED_TIMER_CUMULATIVE_ATOMIC(GMallocDurationCounter, std::micro)

	#define _PROFILE_FREE_CUMULATIVE()                         \
		GFreeCounter.fetch_add(1, std::memory_order_relaxed); \
		HLVM_SCOPED_TIMER_CUMULATIVE_ATOMIC(GFreeDurationCounter, std::micro)
#else
	#define _PROFILE_MALLOC_CUMULATIVE() ((void)0)
	#define _PROFILE_FREE_CUMULATIVE() ((void)0)
#endif

void InitMallocator() // Extern
{
#if !HLVM_BUILD_RELEASE && HLVM_MALLOC_USE_MIMALLOC_OVER_STD
	mi_option_enable(mi_option_t::mi_option_show_errors);
	mi_option_enable(mi_option_t::mi_option_show_stats);
	mi_option_enable(mi_option_t::mi_option_verbose);
#endif
}

struct FMallocatorShutdownCtx
{
	// OPTIONAL : Fill in shutdown ctx
};

void FinlMallocator() // Extern
{
	FMallocatorShutdownCtx Ctx;
	CoreDelegates::OnMallocatorShutdown.Invoke(&Ctx);

	HLVM_LOG(LogMiMallocator, info, TXT("Mallocator summary:\nTotal time on malloc {} micro sec\nNumber of malloc #{}\nPer malloc time {} micro sec\nTotal time on free {} micro sec\nNumber of free #{}\nPer free time {} micro sec"),
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
		HLVM_ENSURE_F(false, TO_TCHAR_CSTR(e.what()));
		return false;
	}
}

#if HLVM_MALLOC_OVERRIDE

/**
 * Overriding new/delete in static/shared lib:
 * https://stackoverflow.com/questions/47372194/integrating-c-custom-memory-allocators-across-shared-static-libraries
 */
	#if !HLVM_ENABLE_GLOBAL_VMMALLOCATOR
		#if HLVM_MALLOC_USE_MIMALLOC_OVER_STD
HLVM_THREAD_LOCAL_VAR IMallocator* GMallocatorTLS = &GMiMallocatorTLS;			// Extern
HLVM_THREAD_LOCAL_VAR IMallocator* GFallBacllMallocatorTLS = &GMiMallocatorTLS; // Extern
		#else
HLVM_THREAD_LOCAL_VAR IMallocator* GMallocatorTLS = &GStdMallocatorTLS;			 // Extern
HLVM_THREAD_LOCAL_VAR IMallocator* GFallBacllMallocatorTLS = &GStdMallocatorTLS; // Extern
		#endif
	#else
HLVM_THREAD_LOCAL_VAR IMallocator* GMallocatorTLS = &GVMArenaMallocatorTLS;			 // Extern
HLVM_THREAD_LOCAL_VAR IMallocator* GFallBacllMallocatorTLS = &GVMArenaMallocatorTLS; // Extern
	#endif

void SwapMallocator(IMallocator* Mallocator) // Extern
{
	if (hlvm_private::GMallocatorTLSSwap == nullptr && Mallocator != nullptr)
	{
		hlvm_private::GMallocatorTLSSwap = GMallocatorTLS;
		GMallocatorTLS = Mallocator;
	}
	else if (Mallocator == nullptr)
	{
		GMallocatorTLS = hlvm_private::GMallocatorTLSSwap;
		hlvm_private::GMallocatorTLSSwap = nullptr;
	}
	else
	{
		HLVM_ENSURE_F(false, TXT("Mallocator swap is not supported in this case"));
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

		#define _NEW_WRAPPER(expr)              \
			void* p = nullptr;                 \
			{                                  \
				_PROFILE_MALLOC_CUMULATIVE();   \
				expr                           \
			}                                  \
			HLVM_PROFILER_CPU_ON_MALLOC(p, n); \
			return p

		#define _TRY_CATCH_NEW(expr_try) \
			try                         \
			{                           \
				expr_try                \
			}                           \
			catch (...)                 \
			{                           \
				p = nullptr;            \
			}

		#define _TRY_CATCH_DELETE(expr_try) \
			try                            \
			{                              \
				expr_try                   \
			}                              \
			catch (...)                    \
			{                              \
			}

		#define _DELETE_WRAPPER(expr)       \
			{                              \
				_PROFILE_FREE_CUMULATIVE(); \
				_TRY_CATCH_DELETE(expr)     \
			}                              \
			HLVM_PROFILER_CPU_ON_FREE(p)

void operator delete(void* p) noexcept
{
	_DELETE_WRAPPER(
		if (GMallocatorTLS->Free(p) != EFreeRetType::Success) {
			HLVM_ENSURE_F(GFallBacllMallocatorTLS->Free(p) == EFreeRetType::Success, TXT("delete failed {}"), p);
		});
};
void operator delete[](void* p) noexcept
{
	_DELETE_WRAPPER(
		if (GMallocatorTLS->Free(p) != EFreeRetType::Success) {
			HLVM_ENSURE_F(GFallBacllMallocatorTLS->Free(p) == EFreeRetType::Success, TXT("delete[] failed {}"), p);
		});
};

void operator delete(void* p, const std::nothrow_t&) noexcept
{
	_DELETE_WRAPPER(
		if (GMallocatorTLS->Free(p) != EFreeRetType::Success) {
			HLVM_ENSURE_F(GFallBacllMallocatorTLS->Free(p) == EFreeRetType::Success, TXT("delete failed {}"), p);
		});
}
void operator delete[](void* p, const std::nothrow_t&) noexcept
{
	_DELETE_WRAPPER(
		if (GMallocatorTLS->Free(p) != EFreeRetType::Success) {
			HLVM_ENSURE_F(GFallBacllMallocatorTLS->Free(p) == EFreeRetType::Success, TXT("delete[] failed {}"), p);
		});
}

mi_decl_new(n) void* operator new(std::size_t n) noexcept(false)
{
	_NEW_WRAPPER(
		n = AlignUp(n, HLVM_MALLOC_ALIGNMENT);
		p = GMallocatorTLS->Malloc(n);
		if (!p) {
			p = GFallBacllMallocatorTLS->Malloc(n);
		} HLVM_ENSURE_F(p, TXT("new failed {}"), n);
		CALLOC(p, n););
}
mi_decl_new(n) void* operator new[](std::size_t n) noexcept(false)
{
	_NEW_WRAPPER(
		n = AlignUp(n, HLVM_MALLOC_ALIGNMENT);
		p = GMallocatorTLS->Malloc(n);
		if (!p) {
			p = GFallBacllMallocatorTLS->Malloc(n);
		} HLVM_ENSURE_F(p, TXT("new[] failed {}"), n);
		CALLOC(p, n););
}

mi_decl_new_nothrow(n) void* operator new(std::size_t n, const std::nothrow_t&) noexcept
{
	_NEW_WRAPPER(
		n = AlignUp(n, HLVM_MALLOC_ALIGNMENT);
		_TRY_CATCH_NEW(
			p = GMallocatorTLS->Malloc(n);
			if (!p) {
				p = GFallBacllMallocatorTLS->Malloc(n);
			} HLVM_ENSURE_F(p, TXT("new failed {}"), n);
			CALLOC(p, n);));
}
mi_decl_new_nothrow(n) void* operator new[](std::size_t n, const std::nothrow_t&) noexcept
{
	_NEW_WRAPPER(
		n = AlignUp(n, HLVM_MALLOC_ALIGNMENT);
		_TRY_CATCH_NEW(
			p = GMallocatorTLS->Malloc2(n);
			if (!p) {
				p = GFallBacllMallocatorTLS->Malloc2(n);
			} HLVM_ENSURE_F(p, TXT("new failed {}"), n);
			CALLOC(p, n);));
}

		#if (__cplusplus >= 201402L || _MSC_VER >= 1916)
void operator delete(void* p, std::size_t n) noexcept
{
	_DELETE_WRAPPER(
		if (GMallocatorTLS->FreeSize(p, n) != EFreeRetType::Success) {
			HLVM_ENSURE_F(GFallBacllMallocatorTLS->FreeSize(p, n) == EFreeRetType::Success, TXT("delete failed {}"), p);
		});
};
void operator delete[](void* p, std::size_t n) noexcept
{
	_DELETE_WRAPPER(
		if (GMallocatorTLS->FreeSize(p, n) != EFreeRetType::Success) {
			HLVM_ENSURE_F(GFallBacllMallocatorTLS->FreeSize(p, n) == EFreeRetType::Success, TXT("delete[] failed {}"), p);
		});
};
		#endif

		#if (__cplusplus > 201402L || defined(__cpp_aligned_new))
void operator delete(void* p, std::align_val_t al) noexcept
{
	_DELETE_WRAPPER(
		size_t align = static_cast<size_t>(al);
		if (GMallocatorTLS->FreeAligned(p, align) != EFreeRetType::Success) {
			HLVM_ENSURE_F(GFallBacllMallocatorTLS->FreeAligned(p, align) == EFreeRetType::Success, TXT("delete failed {}"), p);
		});
}
void operator delete[](void* p, std::align_val_t al) noexcept
{
	_DELETE_WRAPPER(
		size_t align = static_cast<size_t>(al);
		if (GMallocatorTLS->FreeAligned(p, align) != EFreeRetType::Success) {
			HLVM_ENSURE_F(GFallBacllMallocatorTLS->FreeAligned(p, align) == EFreeRetType::Success, TXT("delete[] failed {}"), p);
		});
}
void operator delete(void* p, std::size_t n, std::align_val_t al) noexcept
{
	_DELETE_WRAPPER(
		size_t align = static_cast<size_t>(al);
		if (GMallocatorTLS->FreeSizeAligned(p, n, align) != EFreeRetType::Success) {
			HLVM_ENSURE_F(GFallBacllMallocatorTLS->FreeSizeAligned(p, n, align) == EFreeRetType::Success, TXT("delete failed {}"), p);
		});
};
void operator delete[](void* p, std::size_t n, std::align_val_t al) noexcept
{
	_DELETE_WRAPPER(
		size_t align = static_cast<size_t>(al);
		if (GMallocatorTLS->FreeSizeAligned(p, n, align) != EFreeRetType::Success) {
			HLVM_ENSURE_F(GFallBacllMallocatorTLS->FreeSizeAligned(p, n, align) == EFreeRetType::Success, TXT("delete[] failed {}"), p);
		});
};
void operator delete(void* p, std::align_val_t al, const std::nothrow_t&) noexcept
{
	_DELETE_WRAPPER(
		size_t align = static_cast<size_t>(al);
		if (GMallocatorTLS->FreeAligned(p, align) != EFreeRetType::Success) {
			HLVM_ENSURE_F(GFallBacllMallocatorTLS->FreeAligned(p, align) == EFreeRetType::Success, TXT("delete failed {}"), p);
		});
}
void operator delete[](void* p, std::align_val_t al, const std::nothrow_t&) noexcept
{
	_DELETE_WRAPPER(
		size_t align = static_cast<size_t>(al);
		if (GMallocatorTLS->FreeAligned(p, align) != EFreeRetType::Success) {
			HLVM_ENSURE_F(GFallBacllMallocatorTLS->FreeAligned(p, align) == EFreeRetType::Success, TXT("delete[] failed {}"), p);
		});
}

void* operator new(std::size_t n, std::align_val_t al) noexcept(false)
{
	_NEW_WRAPPER(
		size_t align = static_cast<size_t>(al);
		n = AlignUp(n, align);
		p = GMallocatorTLS->MallocAligned(n, align);
		if (!p) {
			p = GFallBacllMallocatorTLS->MallocAligned(n, align);
		} HLVM_ENSURE_F(p, TXT("new failed {}"), n);
		CALLOC(p, n););
}
void* operator new[](std::size_t n, std::align_val_t al) noexcept(false)
{
	_NEW_WRAPPER(
		size_t align = static_cast<size_t>(al);
		n = AlignUp(n, align);
		p = GMallocatorTLS->MallocAligned(n, align);
		if (!p) {
			p = GFallBacllMallocatorTLS->MallocAligned(n, align);
		} HLVM_ENSURE_F(p, TXT("new[] failed {}"), n);
		CALLOC(p, n););
}
void* operator new(std::size_t n, std::align_val_t al, const std::nothrow_t&) noexcept
{
	_NEW_WRAPPER(
		size_t align = static_cast<size_t>(al);
		n = AlignUp(n, align);
		_TRY_CATCH_NEW(
			p = GMallocatorTLS->MallocAligned2(n, align);
			if (!p) {
				p = GFallBacllMallocatorTLS->MallocAligned2(n, align);
			} HLVM_ENSURE_F(p, TXT("new failed {}"), n);
			CALLOC(p, n);));
}
void* operator new[](std::size_t n, std::align_val_t al, const std::nothrow_t&) noexcept
{
	_NEW_WRAPPER(
		size_t align = static_cast<size_t>(al);
		n = AlignUp(n, align);
		_TRY_CATCH_NEW(
			p = GMallocatorTLS->MallocAligned2(n, align);
			if (!p) {
				p = GFallBacllMallocatorTLS->MallocAligned2(n, align);
			} HLVM_ENSURE_F(p, TXT("new[] failed {}"), n);
			CALLOC(p, n);));
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
