/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "StackMallocator.h"
#include "Core/Log.h"

DELCARE_LOG_CATEGORY(LogMallocator)

#ifndef HVLM_MALLOCATOR_DEATIL_TRACE
	#define HVLM_MALLOCATOR_DEATIL_TRACE 1
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
	HLVM_INLINE_VAR HLVM_STATIC_VAR const TCHAR* sTypeName = TO_TCHAR_STR(typeid(T).name());

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
		T*	 p = nullptr;
		auto realSize = n * sizeof(T);
		if (Mallocator)
		{
			// Using static_cast instead of reinterpret_cast because malloc might return nullptr or NULL
			p = S_C(T*, Mallocator->Malloc(realSize));
		}
		else
		{
			p = std::malloc(realSize);
		}
		if (!p)
			HLVM_UNLIKELY
			{
				throw std::bad_alloc();
			}
#if HVLM_MALLOCATOR_DEATIL_TRACE
		auto _allocSize = hlvm_private::GPMRAllocatedSize.fetch_and(realSize, std::memory_order_relaxed);
		_allocSize += realSize;
		HLVM_LOG(LogMallocator, trace, TXT("Malloc {} {} {} {} {}"),
			sTypeName, n, sizeof(T), realSize, _allocSize);
#endif
		return p;
	}

	void deallocate(T* p, std::size_t n) noexcept
	{
		auto realSize = n * sizeof(T);
		if (Mallocator)
		{
			Mallocator->FreeSize(p, realSize);
		}
		else
		{
			std::free(p);
		}
#if HVLM_MALLOCATOR_DEATIL_TRACE
		auto _allocSize = hlvm_private::GPMRAllocatedSize.fetch_sub(realSize, std::memory_order_relaxed);
		_allocSize -= realSize;
		HLVM_LOG(LogMallocator, trace, TXT("Free {} {} {} {} {}"),
			sTypeName, n, sizeof(T), realSize, _allocSize);
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
