/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Common.h"

#ifndef HLVM_MALLOC_OVERRIDE
	#define HLVM_MALLOC_OVERRIDE 1
#endif

#if HLVM_MALLOC_OVERRIDE

HLVM_TLS_VAR IMallocator* GMallocator = &GMiMllocator;

// Guide to override global new and delete : https://microsoft.github.io/mimalloc/using.html
// MIMALLOC_SHOW_STATS=1 ./Engine/Source/Common/Test/Test3rdParty
// #include <mimalloc-new-delete.h>

	#define HLVM_MIMALLOC_USE() if (GMallocator->Type == EMallocatorType::Mimalloc)
	#define HLVM_MIMALLOC_OWNED(p) if (!GMallocatorSwapped || mi_is_in_heap_region(p))

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
		S_C(FMiMallocator*, GMallocator)->Free(p);
		return;
	}
	GStdMllocator.Free(p);
};
void operator delete[](void* p) noexcept
{
	// hlvm_printf("delete[] %s\n", R_C(uintptr_t, p));
	HLVM_MIMALLOC_OWNED(p)
	{
		S_C(FMiMallocator*, GMallocator)->Free(p);
		return;
	}
	GStdMllocator.Free(p);
};

void operator delete(void* p, const std::nothrow_t&) noexcept
{
	// hlvm_printf("delete %s\n", R_C(uintptr_t, p));
	HLVM_MIMALLOC_OWNED(p)
	{
		S_C(FMiMallocator*, GMallocator)->Free(p);
		return;
	}
	GStdMllocator.Free(p);
}
void operator delete[](void* p, const std::nothrow_t&) noexcept
{
	// hlvm_printf("delete[] %s\n", R_C(uintptr_t, p));
	HLVM_MIMALLOC_OWNED(p)
	{
		S_C(FMiMallocator*, GMallocator)->Free(p);
		return;
	}
	GStdMllocator.Free(p);
}

mi_decl_new(n) void* operator new(std::size_t n) noexcept(false)
{
	// hlvm_printf("new %s\n", n);
	HLVM_MIMALLOC_USE()
	{
		void* p = S_C(FMiMallocator*, GMallocator)->Malloc(n);
		return p;
	}
	void* p = GStdMllocator.Malloc(n);
	return p;
}
mi_decl_new(n) void* operator new[](std::size_t n) noexcept(false)
{
	// hlvm_printf("new[] %s\n", n);
	HLVM_MIMALLOC_USE()
	{
		void* p = S_C(FMiMallocator*, GMallocator)->Malloc(n);
		return p;
	}
	void* p = GStdMllocator.Malloc(n);
	return p;
}

mi_decl_new_nothrow(n) void* operator new(std::size_t n, const std::nothrow_t&) noexcept
{
	// hlvm_printf("new %s\n", n);
	HLVM_MIMALLOC_USE()
	{
		void* p = S_C(FMiMallocator*, GMallocator)->Malloc2(n);
		return p;
	}
	void* p = GStdMllocator.Malloc2(n);
	return p;
}
mi_decl_new_nothrow(n) void* operator new[](std::size_t n, const std::nothrow_t&) noexcept
{
	// hlvm_printf("new[] %s\n", n);
	HLVM_MIMALLOC_USE()
	{
		void* p = S_C(FMiMallocator*, GMallocator)->Malloc2(n);
		return p;
	}
	void* p = GStdMllocator.Malloc2(n);
	return p;
}

		#if (__cplusplus >= 201402L || _MSC_VER >= 1916)
void operator delete(void* p, std::size_t n) noexcept
{
	HLVM_MIMALLOC_OWNED(p)
	{
		S_C(FMiMallocator*, GMallocator)->FreeSize(p, n);
		return;
	}
	GStdMllocator.FreeSize(p, n);
};
void operator delete[](void* p, std::size_t n) noexcept
{
	HLVM_MIMALLOC_OWNED(p)
	{
		S_C(FMiMallocator*, GMallocator)->FreeSize(p, n);
		return;
	}
	GStdMllocator.FreeSize(p, n);
};
		#endif

		#if (__cplusplus > 201402L || defined(__cpp_aligned_new))
void operator delete(void* p, std::align_val_t al) noexcept
{
	size_t n = static_cast<size_t>(al);
	HLVM_MIMALLOC_OWNED(p)
	{
		S_C(FMiMallocator*, GMallocator)->FreeAligned(p, n);
		return;
	}
	GStdMllocator.FreeAligned(p, n);
}
void operator delete[](void* p, std::align_val_t al) noexcept
{
	size_t n = static_cast<size_t>(al);
	HLVM_MIMALLOC_OWNED(p)
	{
		S_C(FMiMallocator*, GMallocator)->FreeAligned(p, n);
		return;
	}
	GStdMllocator.FreeAligned(p, n);
}
void operator delete(void* p, std::size_t n, std::align_val_t al) noexcept
{
	size_t align = static_cast<size_t>(al);
	HLVM_MIMALLOC_OWNED(p)
	{
		S_C(FMiMallocator*, GMallocator)->FreeSizeAligned(p, n, align);
		return;
	}
	GStdMllocator.FreeSizeAligned(p, n, align);
};
void operator delete[](void* p, std::size_t n, std::align_val_t al) noexcept
{
	size_t align = static_cast<size_t>(al);
	HLVM_MIMALLOC_OWNED(p)
	{
		S_C(FMiMallocator*, GMallocator)->FreeSizeAligned(p, n, align);
		return;
	}
	GStdMllocator.FreeSizeAligned(p, n, align);
};
void operator delete(void* p, std::align_val_t al, const std::nothrow_t&) noexcept
{
	size_t n = static_cast<size_t>(al);
	HLVM_MIMALLOC_OWNED(p)
	{
		S_C(FMiMallocator*, GMallocator)->FreeAligned(p, n);
		return;
	}
	GStdMllocator.FreeAligned(p, n);
}
void operator delete[](void* p, std::align_val_t al, const std::nothrow_t&) noexcept
{
	size_t n = static_cast<size_t>(al);
	HLVM_MIMALLOC_OWNED(p)
	{
		S_C(FMiMallocator*, GMallocator)->FreeAligned(p, n);
		return;
	}
	GStdMllocator.FreeAligned(p, n);
}

void* operator new(std::size_t n, std::align_val_t al) noexcept(false)
{
	size_t align = static_cast<size_t>(al);
	HLVM_MIMALLOC_USE()
	{
		void* p = S_C(FMiMallocator*, GMallocator)->MallocAligned(n, align);
		return p;
	}
	void* p = GStdMllocator.MallocAligned(n, align);
	return p;
}
void* operator new[](std::size_t n, std::align_val_t al) noexcept(false)
{
	size_t align = static_cast<size_t>(al);
	HLVM_MIMALLOC_USE()
	{
		void* p = S_C(FMiMallocator*, GMallocator)->MallocAligned(n, align);
		return p;
	}
	void* p = GStdMllocator.MallocAligned(n, align);
	return p;
}
void* operator new(std::size_t n, std::align_val_t al, const std::nothrow_t&) noexcept
{
	size_t align = static_cast<size_t>(al);
	HLVM_MIMALLOC_USE()
	{
		void* p = S_C(FMiMallocator*, GMallocator)->MallocAligned2(n, align);
		return p;
	}
	void* p = GStdMllocator.MallocAligned2(n, align);
	return p;
}
void* operator new[](std::size_t n, std::align_val_t al, const std::nothrow_t&) noexcept
{
	size_t align = static_cast<size_t>(al);
	HLVM_MIMALLOC_USE()
	{
		void* p = S_C(FMiMallocator*, GMallocator)->MallocAligned2(n, align);
		return p;
	}
	void* p = GStdMllocator.MallocAligned2(n, align);
	return p;
}
		#endif

	#endif
	#pragma clang diagnostic pop
#else
HLVM_TLS_VAR IMallocator* GMallocator = GStdMllocator;
#endif
