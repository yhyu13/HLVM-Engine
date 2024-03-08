/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once
#include "GlobalDefinition.h"
#include "Platform/PlatformDefinition.h"
#include "Core/Container/ContainerDefinition.h"
#include "Core/Parallel/ParallelDefinition.h"
#include "Template/GlobalTemplate.tpp"

// https://github.com/microsoft/mimalloc
#include <mimalloc.h>

HLVM_ENUM(EMallocator, uint8_t,
	Mimalloc,
	Stack,
	Unkown);

/**
 * Mallocator interface class
 * Default allocator is Mimalloc
 */
class IMallocator
{
public:
	NOCOPYMOVE(IMallocator)
	IMallocator() = default;
	virtual ~IMallocator() = default;
	HLVM_INLINE_FUNC virtual bool  Owened(void* ptr) noexcept = 0;
	HLVM_INLINE_FUNC virtual void* Malloc(size_t size) noexcept(false) = 0;
	HLVM_INLINE_FUNC virtual void* Malloc2(size_t size) noexcept = 0;
	HLVM_INLINE_FUNC virtual void* MallocAligned(size_t size, size_t alignment) noexcept(false) = 0;
	HLVM_INLINE_FUNC virtual void* MallocAligned2(size_t size, size_t alignment) noexcept = 0;
	HLVM_INLINE_FUNC virtual void  Free(void* ptr) noexcept = 0;
	HLVM_INLINE_FUNC virtual void  FreeSize(void* ptr, size_t size) noexcept = 0;
	HLVM_INLINE_FUNC virtual void  FreeAligned(void* ptr, size_t alignment) noexcept = 0;
	HLVM_INLINE_FUNC virtual void  FreeSizeAligned(void* ptr, size_t size, size_t alignment) noexcept = 0;

	EMallocator Type = EMallocator::Unkown;
};
/**
 * Global mallocator
 */
void									  InitMallocator();
HLVM_TLS_VAR HLVM_EXTERN_VAR IMallocator* GMallocatorTLS;
namespace hlvm_private
{
	HLVM_TLS_VAR HLVM_INLINE_VAR IMallocator* GMallocatorTLSSwap = nullptr;
}
HLVM_INLINE_FUNC void SwapMallocator(IMallocator* Mallocator = nullptr)
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

struct FMiMallocatorContext
{
	bool bNewHeap{ false };
	BIT_FLAG(bDestory){ false }; // CAUTION: Free all allocated heap w/o checking if pages still persist
};
class FMiMallocator final : public IMallocator
{
public:
	NOCOPYMOVE(FMiMallocator)
	FMiMallocator(const FMiMallocatorContext& _Ctx = FMiMallocatorContext())
		: mCtx(_Ctx)
	{
		Type = EMallocator::Mimalloc;
		mHeap = (mCtx.bNewHeap ? mi_heap_new() : mi_heap_get_default());
	}
	~FMiMallocator() final override
	{
		if (mCtx.bNewHeap)
		{
			if (mCtx.bDestory)
				HLVM_UNLIKELY
				{
					mi_heap_destroy(mHeap);
				}
			else
			{
				mi_heap_delete(mHeap);
			}
		}
		else
		{
			mi_heap_collect(mHeap, false);
		}
	}
	virtual bool				   Owened(void* ptr) noexcept final override;
	HLVM_INLINE_FUNC virtual void* Malloc(size_t size) noexcept(false) final override
	{
		return mi_heap_malloc(mHeap, size);
	}
	HLVM_INLINE_FUNC virtual void* Malloc2(size_t size) noexcept final override
	{
		return mi_heap_malloc(mHeap, size);
	}
	HLVM_INLINE_FUNC virtual void* MallocAligned(size_t size, size_t alignment) noexcept(false) final override
	{
		return mi_heap_malloc_aligned(mHeap, size, alignment);
	}
	HLVM_INLINE_FUNC virtual void* MallocAligned2(size_t size, size_t alignment) noexcept final override
	{
		return mi_heap_malloc_aligned(mHeap, size, alignment);
	}
	HLVM_INLINE_FUNC virtual void Free(void* ptr) noexcept final override
	{
		mi_free(ptr);
	}
	HLVM_INLINE_FUNC virtual void FreeSize(void* ptr, size_t size) noexcept final override
	{
		mi_free_size(ptr, size);
	}
	HLVM_INLINE_FUNC virtual void FreeAligned(void* ptr, size_t alignment) noexcept final override
	{
		mi_free_aligned(ptr, alignment);
	}
	HLVM_INLINE_FUNC virtual void FreeSizeAligned(void* ptr, size_t size, size_t alignment) noexcept final override
	{
		mi_free_size_aligned(ptr, size, alignment);
	}

private:
	mi_heap_t*			 mHeap;
	FMiMallocatorContext mCtx;
};
HLVM_TLS_VAR HLVM_INLINE_VAR FMiMallocator GMiMallocatorTLS{};

/**
 * Stack allocator
 */
struct FStackMallocatorContext
{
	bool bMonolithic{ false };
	BIT_FLAG(bDefragmentation){ true };		// Defragment regularly when possible
	BIT_FLAG(bAllowOverflowToHeap){ true }; // Use mimallocator for overflowed allocation
};
template <size_t N = 4 * 1024 * 1024>
class FStackMallocator final : public IMallocator
{
public:
	NOCOPYMOVE(FStackMallocator)
	FStackMallocator(const FStackMallocatorContext& _Ctx = FStackMallocatorContext())
		: mCtx(_Ctx)
	{
		Type = EMallocator::Stack;
		mFreeBlockHead = R_C(FBlock*, mStack);
		*mFreeBlockHead = MoveTemp(FBlock());
		mFreeBlockHead->size = N - 2 * sizeof(FBlock);

		auto Tail = R_C(FBlock*, mStack + N - sizeof(FBlock));
		*Tail = MoveTemp(FBlock());
		Tail->prevFreeBlock = mFreeBlockHead;
		Tail->free = false;

		mFreeBlockHead->nextFreeBlock = Tail;
	}
	~FStackMallocator() final override
	{
	}
	HLVM_INLINE_FUNC virtual bool Owened(void* ptr) noexcept final override
	{
		if (InStackBound(ptr))
		{
			return true;
		}
		else if (mCtx.bAllowOverflowToHeap && mMiMallocator.has_value())
		{
			return mMiMallocator->Owened(ptr);
		}
		else
		{
			return false;
		}
	}
	HLVM_INLINE_FUNC virtual void* Malloc(size_t size) noexcept(false) final override
	{
		return InternalMalloc(size);
	}
	HLVM_INLINE_FUNC virtual void* Malloc2(size_t size) noexcept final override
	{
		return InternalMalloc(size);
	}
	HLVM_INLINE_FUNC virtual void* MallocAligned(size_t size, size_t) noexcept(false) final override
	{
		return InternalMalloc(size);
	}
	HLVM_INLINE_FUNC virtual void* MallocAligned2(size_t size, size_t) noexcept final override
	{
		return InternalMalloc(size);
	}
	HLVM_INLINE_FUNC virtual void Free(void* ptr) noexcept final override
	{
		InternalFree(ptr);
	}
	HLVM_INLINE_FUNC virtual void FreeSize(void* ptr, size_t) noexcept final override
	{
		InternalFree(ptr);
	}
	HLVM_INLINE_FUNC virtual void FreeAligned(void* ptr, size_t) noexcept final override
	{
		InternalFree(ptr);
	}
	HLVM_INLINE_FUNC virtual void FreeSizeAligned(void* ptr, size_t, size_t) noexcept final override
	{
		InternalFree(ptr);
	}

private:
	PACK(struct FBlock {
		FBlock* prevFreeBlock{ nullptr };
		FBlock* nextFreeBlock{ nullptr };
		size_t	size{ 0 };
		BIT_FLAG(free){ true };
	});
	static_assert(2 * sizeof(FBlock) < N);

	void* InternalMalloc(size_t size)
	{
		auto FreeBlock = mFreeBlockHead;
		while (FreeBlock->nextFreeBlock != nullptr)
		{
			if (FreeBlock->size - sizeof(FBlock) >= size)
			{
				auto NextFreeBlock = FreeBlock->nextFreeBlock;
				auto PrevFreeBlock = FreeBlock->prevFreeBlock;

				auto NewFreeBlock = R_C(FBlock*, FreeBlock + sizeof(FBlock) + size);
				*NewFreeBlock = MoveTemp(FBlock());
				NewFreeBlock->size = FreeBlock->size - sizeof(FBlock) - size;
				if (NewFreeBlock->size == 0)
					HLVM_UNLIKELY
					{
						// New free block is trivial, simply ignore it and connect next and prev to each other
						NextFreeBlock->prevFreeBlock = PrevFreeBlock;
						if (PrevFreeBlock)
						{
							PrevFreeBlock->nextFreeBlock = NextFreeBlock;
							// If prev free exists, assign head to prev,
							// and iterate until we reach a head whose prev free block is null
							mFreeBlockHead = PrevFreeBlock;
							while (mFreeBlockHead->prevFreeBlock)
							{
								assert(mFreeBlockHead != mFreeBlockHead->prevFreeBlock);
								mFreeBlockHead = mFreeBlockHead->prevFreeBlock;
							}
						}
						else
						{
							// Otherwise, assign head to next
							mFreeBlockHead = NextFreeBlock;
						}
					}
				else
					HLVM_LIKELY
					{
						NewFreeBlock->nextFreeBlock = NextFreeBlock;
						NewFreeBlock->prevFreeBlock = PrevFreeBlock;
						// Otherwise, connect next and prev to new free block
						NextFreeBlock->prevFreeBlock = NewFreeBlock;
						if (PrevFreeBlock)
						{
							PrevFreeBlock->nextFreeBlock = NewFreeBlock;
							// If prev free exists, assign head to prev,
							// and iterate until we reach a head whose prev free block is null
							mFreeBlockHead = PrevFreeBlock;
							while (mFreeBlockHead->prevFreeBlock)
							{
								assert(mFreeBlockHead != mFreeBlockHead->prevFreeBlock);
								mFreeBlockHead = mFreeBlockHead->prevFreeBlock;
							}
						}
						else
						{
							// Otherwise, assign head to new
							mFreeBlockHead = NewFreeBlock;
						}
					}

				// Allocate current free block
				FreeBlock->size = size;
				FreeBlock->free = false;

				// Return actual pointer address
				return FreeBlock + sizeof(FBlock);
			}
			else
			{
				FreeBlock = FreeBlock->nextFreeBlock;
			}
		}
		// Running out of free blocks in stack, try heap
		if (mCtx.bAllowOverflowToHeap)
		{
			if (!mMiMallocator.has_value())
			{
				mMiMallocator.emplace(FMiMallocatorContext{ .bNewHeap = true, .bDestory = true });
			}
			return mMiMallocator->Malloc(size);
		}
		else
		{
			return nullptr;
		}
	}

	void InternalFree(void* _ptr)
	{
		auto ptr = R_C(std::byte*, _ptr);
		if (InStackBound(ptr))
		{
			if (mCtx.bMonolithic)
			{
				return;
			}
			else
			{
				// TODO , mark free
				FBlock* FreeBlock = R_C(FBlock*, ptr - sizeof(FBlock));
				assert(!FreeBlock->free);
				FreeBlock->free = true;
				FreeBlock->prevFreeBlock = nullptr;

				assert(mFreeBlockHead->prevFreeBlock == nullptr);
				mFreeBlockHead->prevFreeBlock = FreeBlock;
				FreeBlock->nextFreeBlock = mFreeBlockHead;
				mFreeBlockHead = FreeBlock;
				if (mCtx.bDefragmentation)
				{
					// Check next block if free, merge blocks
					FBlock* NextBlock = R_C(FBlock*, FreeBlock + FreeBlock->size + sizeof(FBlock));
					while (NextBlock->free)
					{
						auto NextBlockNextFreeBlock = NextBlock->nextFreeBlock;
						assert(NextBlockNextFreeBlock->prevFreeBlock == NextBlock);
						NextBlockNextFreeBlock->prevFreeBlock = FreeBlock;
						FreeBlock->nextFreeBlock = NextBlockNextFreeBlock;

						auto NextBlockPrevFreeBlock = NextBlock->prevFreeBlock;
						if (NextBlockPrevFreeBlock && NextBlockPrevFreeBlock != FreeBlock)
						{
							assert(NextBlockPrevFreeBlock->nextFreeBlock == NextBlock);
							NextBlockPrevFreeBlock->nextFreeBlock = FreeBlock;
							FreeBlock->prevFreeBlock = NextBlockPrevFreeBlock;
						}

						FreeBlock->size += NextBlock->size + sizeof(FBlock);
						NextBlock = R_C(FBlock*, FreeBlock + FreeBlock->size + sizeof(FBlock));
					}
					// We may have connected non-null block to free head,
					// Iterate until free head's prev is null
					while (mFreeBlockHead->prevFreeBlock)
					{
						assert(mFreeBlockHead != mFreeBlockHead->prevFreeBlock);
						mFreeBlockHead = mFreeBlockHead->prevFreeBlock;
					}
				}
			}
		}
		else if (mCtx.bAllowOverflowToHeap && mMiMallocator.has_value())
		{
			mMiMallocator->Free(ptr);
		}
		else
		{
			GMiMallocatorTLS.Free(ptr);
		}
	}

	bool InStackBound(const void* ptr) const
	{
		return ptr >= mStack + sizeof(FBlock) && ptr < mStack + N - sizeof(FBlock);
	}

	std::byte					 mStack[N];
	FBlock*						 mFreeBlockHead{ nullptr };
	std::optional<FMiMallocator> mMiMallocator{ std::nullopt }; // Use mimallocator when out of stack
	FStackMallocatorContext		 mCtx;
};
