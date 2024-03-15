/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Mallocator/StackMallocator.h"
#include "Core/Log.h"
#include "Platform/GenericPlatformDebuggerUtil.h"
#include "Template/PrintTemplate.tpp"

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
	#define HLVM_MALLOC_USE_GENERAL_PURPOSE_STACK_ALLOCATOR 0
#endif

#ifndef HLVM_MALLOC_USE_CALLOC
	#define HLVM_MALLOC_USE_CALLOC HLVM_BUILD_DEBUG
#endif

#if HLVM_MALLOC_USE_CALLOC
	#define CALLOC(p, n) std::memset(p, 0, n)
#else
	#define CALLOC(...)
#endif

DELCARE_LOG_CATEGORY(LogMiMallocator)

void InitMallocator()
{
#if !HLVM_BUILD_RELEASE
	mi_option_enable(mi_option_t::mi_option_show_errors);
	mi_option_enable(mi_option_t::mi_option_show_stats);
	mi_option_enable(mi_option_t::mi_option_verbose);
#endif
}

bool FMiMallocator::Owened(void* ptr) noexcept
{
	try
	{
		return mi_heap_check_owned(mHeap, ptr);
	}
	catch (std::exception& e)
	{
		const FStdString& Stack = FGenericPlatformDebuggerUtil::GetStackTrace();
		HLVM_LOG(LogMiMallocator, err, TXT("Owened exception : {} at\n{}"), TO_TCHAR_STR(e.what()), *Stack);
		return false;
	}
}

// TODO : throw std::bad_alloc(); on nullptr malloc
#if HLVM_MALLOC_OVERRIDE
HLVM_TLS_VAR IMallocator* GMallocatorTLS = &GMiMallocatorTLS;
void					  SwapMallocator(IMallocator* Mallocator)
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
// #include <mimalloc-new-delete.h>

	#define HLVM_MIMALLOC_USE()                            \
		if (GMallocatorTLS->Type == EMallocator::Mimalloc) \
		HLVM_LIKELY

	#if HLVM_MALLOC_USE_GENERAL_PURPOSE_STACK_ALLOCATOR
		#define HLVM_STACK_USE()                            \
			if (GMallocatorTLS->Type == EMallocator::Stack) \
			HLVM_LIKELY
	#else
		#define HLVM_STACK_USE() \
			if (false)
	#endif

	/**
	 * Mimalloc checks thread local allocated pointer as well as non thread local allocated pointer.
	 * So just let mimalloc does its job on freeing w/o checking owner ship
	 */
	#define HLVM_MIMALLOC_OWNED(p)                         \
		if (GMallocatorTLS->Type == EMallocator::Mimalloc) \
		HLVM_LIKELY

	#if HLVM_MALLOC_USE_GENERAL_PURPOSE_STACK_ALLOCATOR
		#define HLVM_STACK_OWNED(p)                         \
			if (GMallocatorTLS->Type == EMallocator::Stack) \
			HLVM_LIKELY
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
	// hlvm_printf("delete %s\n", R_C(uintptr_t, p));
	HLVM_MIMALLOC_OWNED(p)
	{
		S_C(FMiMallocator*, GMallocatorTLS)->Free(p);
		return;
	}
	HLVM_STACK_OWNED(p)
	{
		GMallocatorTLS->Free(p);
		return;
	}
	GMiMallocatorTLS.Free(p);
};
void operator delete[](void* p) noexcept
{
	// hlvm_printf("delete[] %s\n", R_C(uintptr_t, p));
	HLVM_MIMALLOC_OWNED(p)
	{
		S_C(FMiMallocator*, GMallocatorTLS)->Free(p);
		return;
	}
	HLVM_STACK_OWNED(p)
	{
		GMallocatorTLS->Free(p);
		return;
	}
	GMiMallocatorTLS.Free(p);
};

void operator delete(void* p, const std::nothrow_t&) noexcept
{
	// hlvm_printf("delete %s\n", R_C(uintptr_t, p));
	HLVM_MIMALLOC_OWNED(p)
	{
		S_C(FMiMallocator*, GMallocatorTLS)->Free(p);
		return;
	}
	HLVM_STACK_OWNED(p)
	{
		GMallocatorTLS->Free(p);
		return;
	}
	GMiMallocatorTLS.Free(p);
}
void operator delete[](void* p, const std::nothrow_t&) noexcept
{
	// hlvm_printf("delete[] %s\n", R_C(uintptr_t, p));
	HLVM_MIMALLOC_OWNED(p)
	{
		S_C(FMiMallocator*, GMallocatorTLS)->Free(p);
		return;
	}
	HLVM_STACK_OWNED(p)
	{
		GMallocatorTLS->Free(p);
		return;
	}
	GMiMallocatorTLS.Free(p);
}

mi_decl_new(n) void* operator new(std::size_t n) noexcept(false)
{
	// hlvm_printf("new %s\n", n);
	HLVM_MIMALLOC_USE()
	{
		void* p = S_C(FMiMallocator*, GMallocatorTLS)->Malloc(n);
		CALLOC(p, n);
		return p;
	}
	HLVM_STACK_USE()
	{
		void* p = GMallocatorTLS->Malloc(n);
		CALLOC(p, n);
		return p;
	}
	void* p = GMiMallocatorTLS.Malloc(n);
	CALLOC(p, n);
	return p;
}
mi_decl_new(n) void* operator new[](std::size_t n) noexcept(false)
{
	// hlvm_printf("new[] %s\n", n);
	HLVM_MIMALLOC_USE()
	{
		void* p = S_C(FMiMallocator*, GMallocatorTLS)->Malloc(n);
		CALLOC(p, n);
		return p;
	}
	HLVM_STACK_USE()
	{
		void* p = GMallocatorTLS->Malloc(n);
		CALLOC(p, n);
		return p;
	}
	void* p = GMiMallocatorTLS.Malloc(n);
	CALLOC(p, n);
	return p;
}

mi_decl_new_nothrow(n) void* operator new(std::size_t n, const std::nothrow_t&) noexcept
{
	// hlvm_printf("new %s\n", n);
	HLVM_MIMALLOC_USE()
	{
		void* p = S_C(FMiMallocator*, GMallocatorTLS)->Malloc2(n);
		CALLOC(p, n);
		return p;
	}
	HLVM_STACK_USE()
	{
		void* p = GMallocatorTLS->Malloc2(n);
		CALLOC(p, n);
		return p;
	}
	void* p = GMiMallocatorTLS.Malloc2(n);
	CALLOC(p, n);
	return p;
}
mi_decl_new_nothrow(n) void* operator new[](std::size_t n, const std::nothrow_t&) noexcept
{
	// hlvm_printf("new[] %s\n", n);
	HLVM_MIMALLOC_USE()
	{
		void* p = S_C(FMiMallocator*, GMallocatorTLS)->Malloc2(n);
		CALLOC(p, n);
		return p;
	}
	HLVM_STACK_USE()
	{
		void* p = GMallocatorTLS->Malloc2(n);
		CALLOC(p, n);
		return p;
	}
	void* p = GMiMallocatorTLS.Malloc2(n);
	CALLOC(p, n);
	return p;
}

		#if (__cplusplus >= 201402L || _MSC_VER >= 1916)
void operator delete(void* p, std::size_t n) noexcept
{
	HLVM_MIMALLOC_OWNED(p)
	{
		S_C(FMiMallocator*, GMallocatorTLS)->FreeSize(p, n);
		return;
	}
	HLVM_STACK_OWNED(p)
	{
		GMallocatorTLS->FreeSize(p, n);
		return;
	}
	GMiMallocatorTLS.FreeSize(p, n);
};
void operator delete[](void* p, std::size_t n) noexcept
{
	HLVM_MIMALLOC_OWNED(p)
	{
		S_C(FMiMallocator*, GMallocatorTLS)->FreeSize(p, n);
		return;
	}
	HLVM_STACK_OWNED(p)
	{
		GMallocatorTLS->FreeSize(p, n);
		return;
	}
	GMiMallocatorTLS.FreeSize(p, n);
};
		#endif

		#if (__cplusplus > 201402L || defined(__cpp_aligned_new))
void operator delete(void* p, std::align_val_t al) noexcept
{
	size_t n = static_cast<size_t>(al);
	HLVM_MIMALLOC_OWNED(p)
	{
		S_C(FMiMallocator*, GMallocatorTLS)->FreeAligned(p, n);
		return;
	}
	HLVM_STACK_OWNED(p)
	{
		GMallocatorTLS->FreeAligned(p, n);
		return;
	}
	GMiMallocatorTLS.FreeAligned(p, n);
}
void operator delete[](void* p, std::align_val_t al) noexcept
{
	size_t n = static_cast<size_t>(al);
	HLVM_MIMALLOC_OWNED(p)
	{
		S_C(FMiMallocator*, GMallocatorTLS)->FreeAligned(p, n);
		return;
	}
	HLVM_STACK_OWNED(p)
	{
		GMallocatorTLS->FreeAligned(p, n);
		return;
	}
	GMiMallocatorTLS.FreeAligned(p, n);
}
void operator delete(void* p, std::size_t n, std::align_val_t al) noexcept
{
	size_t align = static_cast<size_t>(al);
	HLVM_MIMALLOC_OWNED(p)
	{
		S_C(FMiMallocator*, GMallocatorTLS)->FreeSizeAligned(p, n, align);
		return;
	}
	HLVM_STACK_OWNED(p)
	{
		GMallocatorTLS->FreeSizeAligned(p, n, align);
		return;
	}
	GMiMallocatorTLS.FreeSizeAligned(p, n, align);
};
void operator delete[](void* p, std::size_t n, std::align_val_t al) noexcept
{
	size_t align = static_cast<size_t>(al);
	HLVM_MIMALLOC_OWNED(p)
	{
		S_C(FMiMallocator*, GMallocatorTLS)->FreeSizeAligned(p, n, align);
		return;
	}
	HLVM_STACK_OWNED(p)
	{
		GMallocatorTLS->FreeSizeAligned(p, n, align);
		return;
	}
	GMiMallocatorTLS.FreeSizeAligned(p, n, align);
};
void operator delete(void* p, std::align_val_t al, const std::nothrow_t&) noexcept
{
	size_t n = static_cast<size_t>(al);
	HLVM_MIMALLOC_OWNED(p)
	{
		S_C(FMiMallocator*, GMallocatorTLS)->FreeAligned(p, n);
		return;
	}
	HLVM_STACK_OWNED(p)
	{
		GMallocatorTLS->FreeAligned(p, n);
		return;
	}
	GMiMallocatorTLS.FreeAligned(p, n);
}
void operator delete[](void* p, std::align_val_t al, const std::nothrow_t&) noexcept
{
	size_t n = static_cast<size_t>(al);
	HLVM_MIMALLOC_OWNED(p)
	{
		S_C(FMiMallocator*, GMallocatorTLS)->FreeAligned(p, n);
		return;
	}
	HLVM_STACK_OWNED(p)
	{
		GMallocatorTLS->FreeAligned(p, n);
		return;
	}
	GMiMallocatorTLS.FreeAligned(p, n);
}

void* operator new(std::size_t n, std::align_val_t al) noexcept(false)
{
	size_t align = static_cast<size_t>(al);
	HLVM_MIMALLOC_USE()
	{
		void* p = S_C(FMiMallocator*, GMallocatorTLS)->MallocAligned(n, align);
		CALLOC(p, n);
		return p;
	}
	HLVM_STACK_USE()
	{
		void* p = GMallocatorTLS->MallocAligned(n, align);
		CALLOC(p, n);
		return p;
	}
	void* p = GMiMallocatorTLS.MallocAligned(n, align);
	CALLOC(p, n);
	return p;
}
void* operator new[](std::size_t n, std::align_val_t al) noexcept(false)
{
	size_t align = static_cast<size_t>(al);
	HLVM_MIMALLOC_USE()
	{
		void* p = S_C(FMiMallocator*, GMallocatorTLS)->MallocAligned(n, align);
		CALLOC(p, n);
		return p;
	}
	HLVM_STACK_USE()
	{
		void* p = GMallocatorTLS->MallocAligned(n, align);
		CALLOC(p, n);
		return p;
	}
	void* p = GMiMallocatorTLS.MallocAligned(n, align);
	CALLOC(p, n);
	return p;
}
void* operator new(std::size_t n, std::align_val_t al, const std::nothrow_t&) noexcept
{
	size_t align = static_cast<size_t>(al);
	HLVM_MIMALLOC_USE()
	{
		void* p = S_C(FMiMallocator*, GMallocatorTLS)->MallocAligned2(n, align);
		CALLOC(p, n);
		return p;
	}
	HLVM_STACK_USE()
	{
		void* p = GMallocatorTLS->MallocAligned2(n, align);
		CALLOC(p, n);
		return p;
	}
	void* p = GMiMallocatorTLS.MallocAligned2(n, align);
	CALLOC(p, n);
	return p;
}
void* operator new[](std::size_t n, std::align_val_t al, const std::nothrow_t&) noexcept
{
	size_t align = static_cast<size_t>(al);
	HLVM_MIMALLOC_USE()
	{
		void* p = S_C(FMiMallocator*, GMallocatorTLS)->MallocAligned2(n, align);
		CALLOC(p, n);
		return p;
	}
	HLVM_STACK_USE()
	{
		void* p = GMallocatorTLS->MallocAligned2(n, align);
		CALLOC(p, n);
		return p;
	}
	void* p = GMiMallocatorTLS.MallocAligned2(n, align);
	CALLOC(p, n);
	return p;
}
		#endif

	#endif
	#pragma clang diagnostic pop

#else

HLVM_TLS_VAR IMallocator* GMallocatorTLS = nullptr;
void					  SwapMallocator(IMallocator*)
{
}

#endif
