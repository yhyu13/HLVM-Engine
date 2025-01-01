/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "VMArena.h"

template <TUINT8 Alignment>
class FSmallBinnedMallocator final : public ISmallBinnedMallocator
{
	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr bool	   bValidate = HLVM_MALLOC_VALIDATION;
	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr uint32_t BlockRunOutFreeIndex = 32;

	static_assert(Alignment > 0);

public:
	NOCOPYMOVE(FSmallBinnedMallocator)
	FSmallBinnedMallocator() = default;
	virtual ~FSmallBinnedMallocator() noexcept final override
	{
		while (mFirstBlocks32)
		{
			FBlocks32* Next = mFirstBlocks32->Next;
			Mallocator->FreeHeap(mFirstBlocks32);
			mFirstBlocks32 = Next;
		}
	}

	virtual void Init(FVMArena* _Mallocator) final override
	{
		Mallocator = _Mallocator;
	}

	virtual void* Malloc() final override
	{
		// Allocate first block
		if (!mFirstBlocks32)
		{
			mFirstBlocks32 = InternalAllocateBlocks32();
			mLastFreedBlocks32 = mFirstBlocks32;
		}

		// Get free index
		uint32_t freeIndex = mLastFreedBlocks32->GetHighestFreeBit();
		if (freeIndex == BlockRunOutFreeIndex)
		{
			// If no free block , we allocate new block
			if (!mLastFreedBlocks32->Next)
			{
				mLastFreedBlocks32->Next = InternalAllocateBlocks32();
			}
			mLastFreedBlocks32 = mLastFreedBlocks32->Next;
			freeIndex = 31;
			HLVM_CONSTEXPR_ASSERT(bValidate, mLastFreedBlocks32->GetHighestFreeBit() == 31);
		}

		// Mask allocated index
		mLastFreedBlocks32->FreeBits &= ~(1u << freeIndex);
		// Setup block head
		FSmallBinnedBlockHead* BlockHead = R_C(FSmallBinnedBlockHead*, R_C(TBYTE*, mLastFreedBlocks32->P) + freeIndex * (Alignment + sizeof(FSmallBinnedBlockHead)));
		BlockHead->Pos = S_C(TUINT8, freeIndex);
		BlockHead->Alignment = Alignment;
		// Return actual pointer address
		auto RetPtr = R_C(TBYTE*, BlockHead) + sizeof(FSmallBinnedBlockHead);
		return RetPtr;
	}

	virtual void Free(void* p) noexcept final override
	{
		FSmallBinnedBlockHead* BlockHead = R_C(FSmallBinnedBlockHead*, R_C(TBYTE*, p) - sizeof(FSmallBinnedBlockHead));
		HLVM_CONSTEXPR_ASSERT(bValidate, BlockHead->Alignment == Alignment);
		FBlocks32* Block = R_C(FBlocks32*, R_C(TBYTE*, BlockHead) - BlockHead->Pos * (BlockHead->Alignment + sizeof(FSmallBinnedBlockHead)));
		if constexpr (bValidate)
		{
			/**
			 * Validate Block is within our owned
			 */
			auto Temp = mFirstBlocks32;
			while (Temp != nullptr)
			{
				if (Temp == Block)
				{
					break;
				}
				Temp = Temp->Next;
			}
			assert(Temp != nullptr);
		}
		mLastFreedBlocks32 = Block;
		// Mask free bit
		mLastFreedBlocks32->FreeBits |= (1u << BlockHead->Pos);
	}

private:
	struct MS_ALIGN(HLVM_MALLOC_ALIGNMENT) FBlocks32
	{
		TBYTE	   P[((Alignment + sizeof(FSmallBinnedBlockHead)) << 5)]; // P should point to some mallocated memory that is 32 * (Alignment + sizeof(FSmallBinnedBlockHead)) bytes in size
		FBlocks32* Next;												  // Point to the Next FBlocks32
		uint32_t   FreeBits;											  // Use 32 bits to store free bits for each one of 32 blocks

		/**
		 * Fast bit operation to get highest log2 bit
		 * https://graphics.stanford.edu/~seander/bithacks.html#IntegerLogIEEE64Float
		 */
		uint32_t GetHighestFreeBit() const
		{
			if (FreeBits == 0)
			{
				/**
				 * Since when FreeBits == 0 or ==1, r could be 0 for both cases,
				 * we need to handle FreeBits == 0 specially
				 */
				return BlockRunOutFreeIndex;
			}
			uint32_t v = FreeBits;
			uint32_t r; // result of log2(v) will go here
			uint32_t shift;

			r = S_C(uint32_t, v > 0xFFFF) << 4;
			v >>= r;
			shift = S_C(uint32_t, v > 0xFF) << 3;
			v >>= shift;
			r |= shift;
			shift = S_C(uint32_t, v > 0xF) << 2;
			v >>= shift;
			r |= shift;
			shift = S_C(uint32_t, v > 0x3) << 1;
			v >>= shift;
			r |= shift;
			r |= (v >> 1);
			return r;
		}

		bool Owned(void* v) const
		{
			return v >= R_C(TBYTE*, P) + sizeof(FSmallBinnedBlockHead) && v < R_C(TBYTE*, P) + ((Alignment + sizeof(FSmallBinnedBlockHead)) << 5);
		}
		bool CheckFree(void* v) const
		{
			FSmallBinnedBlockHead* Block = R_C(FSmallBinnedBlockHead*, R_C(TBYTE*, v) - sizeof(FSmallBinnedBlockHead));
			return ((1u << Block->Pos) | FreeBits) == 0;
		}
	} GCC_ALIGN(HLVM_MALLOC_ALIGNMENT);

	FBlocks32* InternalAllocateBlocks32()
	{
		FBlocks32* Temp = R_C(FBlocks32*, Mallocator->MallocHeap(sizeof(FBlocks32)));
		Temp->FreeBits = 0xFFFFFFFF;
		Temp->Next = nullptr;
		return Temp;
	}

	FBlocks32* mFirstBlocks32{ nullptr };
	FBlocks32* mLastFreedBlocks32{ nullptr }; // Cache last freed block to faster malloc next time
	FVMArena*  Mallocator{ nullptr };
};
