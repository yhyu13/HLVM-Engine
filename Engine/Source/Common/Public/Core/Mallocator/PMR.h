/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "StackMallocator.h"
#include "Core/Log.h"

DELCARE_LOG_CATEGORY(LogPMR)

#ifndef HVLM_MALLOCATOR_DEATIL_TRACE
	#define HVLM_MALLOCATOR_DEATIL_TRACE HLVM_BUILD_DEBUG
#endif

#if HVLM_MALLOCATOR_DEATIL_TRACE
namespace hlvm_private
{
	HLVM_INLINE_VAR std::atomic_size_t GPMRAllocatedSize;
}
#endif

/*
	Reference https://en.cppreference.com/w/cpp/named_req/Allocator
*/
template <class T>
struct TMallocator
{
	HLVM_INLINE_VAR HLVM_STATIC_VAR FString sTypeName{ typeid(T).name() };

	using value_type = T;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	using propagate_on_container_move_assignment = std::true_type;
	using is_always_equal = std::true_type;

	TMallocator() noexcept = default;
	TMallocator(const TMallocator&) noexcept = default;

	/**
	 * @param _Mallocator External mallocator, mimallocator or stack allocator
	 */
	explicit TMallocator(IMallocator* _Mallocator) noexcept
		: Mallocator(_Mallocator)
	{
	}

	template <class U>
	TMallocator(const TMallocator<U>&) noexcept
	{
	}

	HLVM_NODISCARD T* allocate(std::size_t n)
	{
		void*  p = nullptr;
		size_t realSize = n * sizeof(T);
		if (Mallocator)
		{
			// Using static_cast instead of reinterpret_cast because malloc might return nullptr or NULL
			p = Mallocator->Malloc(realSize);
		}
		else
		{
			p = new TBYTE[realSize];
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

	void deallocate(T* _p, std::size_t n) noexcept
	{
		void*  p = R_C(void*, _p);
		size_t realSize = n * sizeof(T);
		if (Mallocator)
		{
			Mallocator->FreeSize(p, realSize);
		}
		else
		{
			delete[] R_C(TBYTE*, p);
		}
#if HVLM_MALLOCATOR_DEATIL_TRACE
		size_t _allocSize = hlvm_private::GPMRAllocatedSize.fetch_sub(realSize, std::memory_order_relaxed);
		_allocSize -= realSize;
		HLVM_LOG(LogPMR, trace, TXT("Free {} {} {} {:p} {}"),
			*sTypeName, n, sizeof(T), p, _allocSize);
#endif
	}

private:
	IMallocator* Mallocator{ nullptr };
};

template <class W, class U>
inline bool operator==(const TMallocator<W>&, const TMallocator<U>&)
{
	return true;
}

template <class W, class U>
inline bool operator!=(const TMallocator<W>&, const TMallocator<U>&)
{
	return false;
}
