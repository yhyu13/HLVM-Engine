/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "MiMallocator.h"
#include "StdMallocator.h"
#include "StackMallocator.h"
#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogPMR)

#ifndef HVLM_MALLOCATOR_DEATIL_TRACE
	#define HVLM_MALLOCATOR_DEATIL_TRACE 0
#endif

#if HVLM_MALLOCATOR_DEATIL_TRACE
namespace hlvm_private
{
	HLVM_INLINE_VAR std::atomic_size_t GPMRAllocatedSize;
}
#endif

// Define the PMRMAllocator concept
template <typename T, typename U>
concept PMRMallocator = requires(T&& t, std::size_t n) {
	{
		t.allocate(n)
	} -> std::convertible_to<U*>;
} && requires(T&& t, U* p, std::size_t n) {
	{
		t.deallocate(p, n)
	};
};

/*
	Reference https://en.cppreference.com/w/cpp/named_req/Allocator
*/
template <class T, bool bForceAlignedAlloc = false>
struct TPMRStdMallocator
{
#if HVLM_MALLOCATOR_DEATIL_TRACE
	HLVM_INLINE_VAR HLVM_STATIC_VAR FString sTypeName{ typeid(T).name() };
#endif

	using value_type = T;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	using propagate_on_container_move_assignment = std::true_type;
	using is_always_equal = std::true_type;

	template <class U>
	struct rebind
	{
		using other = TPMRStdMallocator<U, bForceAlignedAlloc>;
	};

	TPMRStdMallocator() noexcept = default;
	TPMRStdMallocator(const TPMRStdMallocator&) noexcept = default;

	template <class U>
	TPMRStdMallocator(const TPMRStdMallocator<U>&) noexcept
	{
	}

	HLVM_NODISCARD T* allocate(std::size_t n = 1)
	{
		void*  p = nullptr;
		size_t realSize = n * sizeof(T);
		{
			if constexpr (bForceAlignedAlloc)
			{
				p = new (static_cast<std::align_val_t>(alignof(T))) TBYTE[realSize];
			}
			else
			{
				p = new TBYTE[realSize];
			}
		}
		if (!p)
			HLVM_UNLIKELY
			{
				throw std::bad_alloc();
			}
#if HVLM_MALLOCATOR_DEATIL_TRACE
		size_t _allocSize = hlvm_private::GPMRAllocatedSize.fetch_add(realSize, std::memory_order_relaxed);
		_allocSize += realSize;
		HLVM_LOG(LogPMR, trace, TXT("Malloc {} {} {} {:p} {}"),
			*sTypeName, n, sizeof(T), p, _allocSize);
#endif
		return R_C(T*, p);
	}

	void deallocate(T* _p, std::size_t n = 1) noexcept
	{
		void*					p = R_C(void*, _p);
		[[maybe_unused]] size_t realSize = n * sizeof(T);
		{
			if constexpr (bForceAlignedAlloc)
			{
				// See https://stackoverflow.com/a/47848630/6658943 on how to use delete placements
				operator delete[](R_C(TBYTE*, p), (static_cast<std::align_val_t>(alignof(T))));
			}
			else
			{
				delete[] (R_C(TBYTE*, p));
			}
		}
#if HVLM_MALLOCATOR_DEATIL_TRACE
		size_t _allocSize = hlvm_private::GPMRAllocatedSize.fetch_sub(realSize, std::memory_order_relaxed);
		_allocSize -= realSize;
		HLVM_LOG(LogPMR, trace, TXT("Free {} {} {} {:p} {}"),
			*sTypeName, n, sizeof(T), p, _allocSize);
#endif
	}
};

template <class W, class U>
inline bool operator==(const TPMRStdMallocator<W>&, const TPMRStdMallocator<U>&)
{
	return true;
}

template <class W, class U>
inline bool operator!=(const TPMRStdMallocator<W>&, const TPMRStdMallocator<U>&)
{
	return false;
}

/*
	Reference https://en.cppreference.com/w/cpp/named_req/Allocator
*/
template <class T, bool bForceAlignedAlloc = false>
struct TPMRIMallocator
{
#if HVLM_MALLOCATOR_DEATIL_TRACE
	HLVM_INLINE_VAR HLVM_STATIC_VAR FString sTypeName{ typeid(T).name() };
#endif

	using value_type = T;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	using propagate_on_container_move_assignment = std::true_type;
	using is_always_equal = std::true_type;

	template <class U>
	struct rebind
	{
		using other = TPMRIMallocator<U, bForceAlignedAlloc>;
	};

	TPMRIMallocator() noexcept = delete;
	TPMRIMallocator(const TPMRIMallocator&) noexcept = default;

	/**
	 * @param _Mallocator External mallocator, mimallocator or stack allocator
	 */
	explicit TPMRIMallocator(IMallocator* _Mallocator) noexcept
		: Mallocator(_Mallocator)
	{
	}

	template <class U>
	TPMRIMallocator(const TPMRIMallocator<U>&) noexcept
	{
	}

	HLVM_NODISCARD T* allocate(std::size_t n = 1)
	{
		void*  p = nullptr;
		size_t realSize = n * sizeof(T);
		{
			// Using static_cast instead of reinterpret_cast because malloc might return nullptr or NULL
			if constexpr (bForceAlignedAlloc)
			{
				p = Mallocator->MallocAligned(realSize, alignof(T));
			}
			else
			{
				p = Mallocator->Malloc(realSize);
			}
		}
		if (!p)
			HLVM_UNLIKELY
			{
				throw std::bad_alloc();
			}
#if HVLM_MALLOCATOR_DEATIL_TRACE
		size_t _allocSize = hlvm_private::GPMRAllocatedSize.fetch_add(realSize, std::memory_order_relaxed);
		_allocSize += realSize;
		HLVM_LOG(LogPMR, trace, TXT("Malloc {} {} {} {:p} {}"),
			*sTypeName, n, sizeof(T), p, _allocSize);
#endif
		return R_C(T*, p);
	}

	void deallocate(T* _p, std::size_t n = 1) noexcept
	{
		void*  p = R_C(void*, _p);
		size_t realSize = n * sizeof(T);
		{
			if constexpr (bForceAlignedAlloc)
			{
				HLVM_ENSURE(Mallocator->FreeSizeAligned(p, realSize, alignof(T)) == EFreeRetType::Success,
					TXT("aligned deallocate failed {}"), p);
			}
			else
			{
				HLVM_ENSURE(Mallocator->FreeSize(p, realSize) == EFreeRetType::Success,
					TXT("deallocate failed {}"), p);
			}
		}
#if HVLM_MALLOCATOR_DEATIL_TRACE
		size_t _allocSize = hlvm_private::GPMRAllocatedSize.fetch_sub(realSize, std::memory_order_relaxed);
		_allocSize -= realSize;
		HLVM_LOG(LogPMR, trace, TXT("Free {} {} {} {:p} {}"),
			*sTypeName, n, sizeof(T), p, _allocSize);
#endif
	}

private:
	TNoNullPointer<IMallocator> Mallocator;
};

template <class W, class U>
inline bool operator==(const TPMRIMallocator<W>&, const TPMRIMallocator<U>&)
{
	return true;
}

template <class W, class U>
inline bool operator!=(const TPMRIMallocator<W>&, const TPMRIMallocator<U>&)
{
	return false;
}

/*
	Reference https://en.cppreference.com/w/cpp/named_req/Allocator
*/
template <class T, bool bForceAlignedAlloc = false>
struct TPMRGMallocator
{
#if HVLM_MALLOCATOR_DEATIL_TRACE
	HLVM_INLINE_VAR HLVM_STATIC_VAR FString sTypeName{ typeid(T).name() };
#endif

	using value_type = T;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	using propagate_on_container_move_assignment = std::true_type;
	using is_always_equal = std::true_type;

	template <class U>
	struct rebind
	{
		using other = TPMRGMallocator<U>;
	};

	TPMRGMallocator() noexcept = default;
	TPMRGMallocator(const TPMRGMallocator&) noexcept = default;

	template <class U>
	TPMRGMallocator(const TPMRGMallocator<U>&) noexcept
	{
	}

	HLVM_NODISCARD T* allocate(std::size_t n = 1)
	{
		void*  p = nullptr;
		size_t realSize = n * sizeof(T);
		// Using static_cast instead of reinterpret_cast because malloc might return nullptr or NULL
		if constexpr (bForceAlignedAlloc)
		{
			p = HLVM_LOWLEVEL_GMALLOCATOR.MallocAligned(realSize, alignof(T));
		}
		else
		{
			p = HLVM_LOWLEVEL_GMALLOCATOR.Malloc(realSize);
		}
		if (!p)
			HLVM_UNLIKELY
			{
				throw std::bad_alloc();
			}
#if HVLM_MALLOCATOR_DEATIL_TRACE
		size_t _allocSize = hlvm_private::GPMRAllocatedSize.fetch_add(realSize, std::memory_order_relaxed);
		_allocSize += realSize;
		HLVM_LOG(LogPMR, trace, TXT("Malloc {} {} {} {:p} {}"),
			*sTypeName, n, sizeof(T), p, _allocSize);
#endif
		return R_C(T*, p);
	}

	void deallocate(T* _p, std::size_t n = 1) noexcept
	{
		void*  p = R_C(void*, _p);
		size_t realSize = n * sizeof(T);
		if constexpr (bForceAlignedAlloc)
		{
			HLVM_ENSURE(HLVM_LOWLEVEL_GMALLOCATOR.FreeSizeAligned(p, realSize, alignof(T)) == EFreeRetType::Success,
				TXT("deallocate failed {}"), p);
		}
		else
		{
			HLVM_ENSURE(HLVM_LOWLEVEL_GMALLOCATOR.FreeSize(p, realSize) == EFreeRetType::Success,
				TXT("deallocate failed {}"), p);
		}
#if HVLM_MALLOCATOR_DEATIL_TRACE
		size_t _allocSize = hlvm_private::GPMRAllocatedSize.fetch_sub(realSize, std::memory_order_relaxed);
		_allocSize -= realSize;
		HLVM_LOG(LogPMR, trace, TXT("Free {} {} {} {:p} {}"),
			*sTypeName, n, sizeof(T), p, _allocSize);
#endif
	}
};

template <class W, class U>
inline bool operator==(const TPMRGMallocator<W>&, const TPMRGMallocator<U>&)
{
	return true;
}

template <class W, class U>
inline bool operator!=(const TPMRGMallocator<W>&, const TPMRGMallocator<U>&)
{
	return false;
}
