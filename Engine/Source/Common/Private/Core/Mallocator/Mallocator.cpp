/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Mallocator/MiMallocator.h"
#include "Core/Mallocator/StdMallocator.h"
#include "Core/Mallocator/StackMallocator.h"
#include "Core/Log.h"
#include "Platform/GenericPlatformStackTrace.h"
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
	#define CALLOC(...)
#endif

DECLARE_LOG_CATEGORY(LogMiMallocator)

void InitMallocator()
{
#if !HLVM_BUILD_RELEASE && HLVM_MALLOC_USE_MIMALLOC_OVER_STD
	mi_option_enable(mi_option_t::mi_option_show_errors);
	mi_option_enable(mi_option_t::mi_option_show_stats);
	mi_option_enable(mi_option_t::mi_option_verbose);
#endif
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

// TODO : throw std::bad_alloc(); on nullptr malloc
#if HLVM_MALLOC_OVERRIDE
HLVM_TLS_VAR IMallocator* GMallocatorTLS = &GMiMallocatorTLS;
HLVM_TLS_VAR IMallocator* GFallBacllMallocatorTLS = &GMiMallocatorTLS;

void SwapMallocator(IMallocator* Mallocator)
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

	#if HLVM_MALLOC_USE_MIMALLOC_OVER_STD
		#define HLVM_MIMALLOC_USE() \
			if (GMallocatorTLS->Type == EMallocator::Mimalloc)
	#else
		#define HLVM_MIMALLOC_USE() \
			if (false)
	#endif

	#if HLVM_MALLOC_USE_GENERAL_PURPOSE_STACK_ALLOCATOR
		#define HLVM_STACK_USE() \
			if (GMallocatorTLS->Type == EMallocator::Stack)
	#else
		#define HLVM_STACK_USE() \
			if (false)
	#endif

/**
 * Mimalloc checks thread local allocated pointer as well as non thread local allocated pointer.
 * So just let mimalloc does its job on freeing w/o checking owner ship
 */
	#if HLVM_MALLOC_USE_MIMALLOC_OVER_STD
		#define HLVM_MIMALLOC_OWNED(p) \
			if (GMallocatorTLS->Type == EMallocator::Mimalloc)
	#else
		#define HLVM_MIMALLOC_OWNED() \
			if (false)
	#endif

	#if HLVM_MALLOC_USE_GENERAL_PURPOSE_STACK_ALLOCATOR
		#define HLVM_STACK_OWNED(p) \
			if (GMallocatorTLS->Type == EMallocator::Stack && GMallocatorTLS->Owned(p))
	#else
		#define HLVM_STACK_OWNED(p) \
			if (false)
	#endif

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
	// StreamPrintf(&std::cout, "delete %s\n", R_C(uintptr_t, p));
	if (GMallocatorTLS->Free(p) != EFreeRetType::Success)
	{
		HLVM_ENSURE(GFallBacllMallocatorTLS->Free(p), TXT("delete failed {}"), p);
	}
};
void operator delete[](void* p) noexcept
{
	// StreamPrintf(&std::cout, "delete[] %s\n", R_C(uintptr_t, p));
	if (GMallocatorTLS->Free(p) != EFreeRetType::Success)
	{
		HLVM_ENSURE(GFallBacllMallocatorTLS->Free(p), TXT("delete failed {}"), p);
	}
};

void operator delete(void* p, const std::nothrow_t&) noexcept
{
	// StreamPrintf(&std::cout, "delete %s\n", R_C(uintptr_t, p));
	if (GMallocatorTLS->Free(p) != EFreeRetType::Success)
	{
		HLVM_ENSURE(GFallBacllMallocatorTLS->Free(p), TXT("delete failed {}"), p);
	}
}
void operator delete[](void* p, const std::nothrow_t&) noexcept
{
	// StreamPrintf(&std::cout, "delete[] %s\n", R_C(uintptr_t, p));
	if (GMallocatorTLS->Free(p) != EFreeRetType::Success)
	{
		HLVM_ENSURE(GFallBacllMallocatorTLS->Free(p), TXT("delete failed {}"), p);
	}
}

mi_decl_new(n) void* operator new(std::size_t n) noexcept(false)
{
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
mi_decl_new_nothrow(n) void* operator new[](std::size_t n, const std::nothrow_t&) noexcept
{
	// StreamPrintf(&std::cout, "new[] %s\n", n);
	n = AlignUp(n, HLVM_MALLOC_ALIGNMENT);
	void* p = GMallocatorTLS->Malloc2(n);
	if (!p)
	{
		p = GFallBacllMallocatorTLS->Malloc2(n);
	}
	HLVM_ENSURE(p, TXT("new failed {}"), n);
	CALLOC(p, n);
	return p;
}

		#if (__cplusplus >= 201402L || _MSC_VER >= 1916)
void operator delete(void* p, std::size_t n) noexcept
{
	if (GMallocatorTLS->FreeSize(p, n) != EFreeRetType::Success)
	{
		HLVM_ENSURE(GFallBacllMallocatorTLS->FreeSize(p, n), TXT("delete failed {}"), p);
	}
};
void operator delete[](void* p, std::size_t n) noexcept
{
	if (GMallocatorTLS->FreeSize(p, n) != EFreeRetType::Success)
	{
		HLVM_ENSURE(GFallBacllMallocatorTLS->FreeSize(p, n), TXT("delete failed {}"), p);
	}
};
		#endif

		#if (__cplusplus > 201402L || defined(__cpp_aligned_new))
void operator delete(void* p, std::align_val_t al) noexcept
{
	size_t align = static_cast<size_t>(al);
	if (GMallocatorTLS->FreeAligned(p, align) != EFreeRetType::Success)
	{
		HLVM_ENSURE(GFallBacllMallocatorTLS->FreeAligned(p, align), TXT("delete failed {}"), p);
	}
}
void operator delete[](void* p, std::align_val_t al) noexcept
{
	size_t align = static_cast<size_t>(al);
	if (GMallocatorTLS->FreeAligned(p, align) != EFreeRetType::Success)
	{
		HLVM_ENSURE(GFallBacllMallocatorTLS->FreeAligned(p, align), TXT("delete failed {}"), p);
	}
}
void operator delete(void* p, std::size_t n, std::align_val_t al) noexcept
{
	size_t align = static_cast<size_t>(al);
	if (GMallocatorTLS->FreeSizeAligned(p, n, align) != EFreeRetType::Success)
	{
		HLVM_ENSURE(GFallBacllMallocatorTLS->FreeSizeAligned(p, n, align), TXT("delete failed {}"), p);
	}
};
void operator delete[](void* p, std::size_t n, std::align_val_t al) noexcept
{
	size_t align = static_cast<size_t>(al);
	if (GMallocatorTLS->FreeSizeAligned(p, n, align) != EFreeRetType::Success)
	{
		HLVM_ENSURE(GFallBacllMallocatorTLS->FreeSizeAligned(p, n, align), TXT("delete failed {}"), p);
	}
};
void operator delete(void* p, std::align_val_t al, const std::nothrow_t&) noexcept
{
	size_t align = static_cast<size_t>(al);
	if (GMallocatorTLS->FreeAligned(p, align) != EFreeRetType::Success)
	{
		HLVM_ENSURE(GFallBacllMallocatorTLS->FreeAligned(p, align), TXT("delete failed {}"), p);
	}
}
void operator delete[](void* p, std::align_val_t al, const std::nothrow_t&) noexcept
{
	size_t align = static_cast<size_t>(al);
	if (GMallocatorTLS->FreeAligned(p, align) != EFreeRetType::Success)
	{
		HLVM_ENSURE(GFallBacllMallocatorTLS->FreeAligned(p, align), TXT("delete failed {}"), p);
	}
}

void* operator new(std::size_t n, std::align_val_t al) noexcept(false)
{
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
	size_t align = static_cast<size_t>(al);
	n = AlignUp(n, align);
	void* p = GMallocatorTLS->MallocAligned2(n, align);
	if (!p)
	{
		p = GFallBacllMallocatorTLS->MallocAligned2(n, align);
	}
	HLVM_ENSURE(p, TXT("new failed {}"), n);
	CALLOC(p, n);
	return p;
}
void* operator new[](std::size_t n, std::align_val_t al, const std::nothrow_t&) noexcept
{
	size_t align = static_cast<size_t>(al);
	n = AlignUp(n, align);
	void* p = GMallocatorTLS->MallocAligned2(n, align);
	if (!p)
	{
		p = GFallBacllMallocatorTLS->MallocAligned2(n, align);
	}
	HLVM_ENSURE(p, TXT("new failed {}"), n);
	CALLOC(p, n);
	return p;
}
		#endif

	#endif
	#pragma clang diagnostic pop

#else

HLVM_TLS_VAR IMallocator* GMallocatorTLS = nullptr;
HLVM_TLS_VAR IMallocator* GFallBacllMallocatorTLS = nullptr;

void SwapMallocator(IMallocator*)
{
}

#endif
