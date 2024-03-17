/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Mallocator/IMallocator.h"

#ifndef HLVM_SMALL_ALLOC_THRESHOLD
	#define HLVM_SMALL_ALLOC_THRESHOLD 128 // Must be smaller than 256
#endif

#ifndef HLVM_SMALL_ALLOC_ALIGNMENT
	#define HLVM_SMALL_ALLOC_ALIGNMENT 16
#endif

struct FSmallBinnedBlockHead
{
	// Use this FSmallBinnedBlockHead* - Pos * Alignment to get FBlocks32's P pointer location
	uint8_t Pos;
	uint8_t Alignment;

	bool Valid() const
	{
		return Pos <= 32 && (Alignment & (Alignment - 1)) == 0 && Alignment <= HLVM_SMALL_ALLOC_THRESHOLD && Alignment > 0;
	}
	HLVM_STATIC_FUNC uint8_t PotentiallyOwned(void* v);
	HLVM_STATIC_FUNC uint8_t GoodSize(size_t _size)
	{
		uint8_t size = S_C(uint8_t, _size);
		// Round to next 16 multiplier
		uint8_t remainder = size % HLVM_SMALL_ALLOC_ALIGNMENT;
		if (remainder == 0)
		{
			return size;
		}
		else
		{
			return size + (HLVM_SMALL_ALLOC_ALIGNMENT - remainder);
		}
	}
};
static_assert(sizeof(FSmallBinnedBlockHead) == 2);

PACK(struct FBlocks32 {
	void*	   P{ nullptr };		   // P should point to some mallocated memory that is 32 * (Alignment + sizeof(FSmallBinnedBlockHead)) bytes in size
	FBlocks32* Next{ nullptr };		   // Point to the Next FBlocks32
	uint32_t   FreeBits{ 0xFFFFFFFF }; // Use 32 bits to store free bits for each one of 32 blocks
	uint8_t	   Alignment{ 0 };

	size_t GetHighestFreeBit() const
	{
		if (FreeBits == 0)
		{
			return 32;
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
		return (1u << Block->Pos | FreeBits) == 0;
	}
});

HLVM_INLINE_FUNC uint8_t FSmallBinnedBlockHead::PotentiallyOwned(void* v)
{
	FSmallBinnedBlockHead* BlockHead = R_C(FSmallBinnedBlockHead*, R_C(TBYTE*, v) - sizeof(FSmallBinnedBlockHead));
	if (BlockHead->Valid())
	{
		return BlockHead->Alignment;
	}
	return 0;
}

/**
 * Inspired by mimalloc small allocation,
 * We implement a binnned mallocator that is designed for small allocation under 255 bytes
 * With 32 items in each bin, and bins chains together
 *
 * Call FSmallBinnedBlockHead::GoodeSize to determine a good Alignment size for small allocation
 */
class FVMArena;
class FSmallBinnedMallocator
{
	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr bool bValidate = HLVM_MALLOC_VALIDATION;

public:
	NOCOPYMOVE(FSmallBinnedMallocator)
	FSmallBinnedMallocator() = default;
	~FSmallBinnedMallocator() noexcept;

	void Init(FVMArena* _Mallocator, uint8_t _Alignment)
	{
		Mallocator = _Mallocator;
		Alignment = _Alignment;
#if HLVM_MALLOC_VALIDATION
		assert(Alignment > 0);
		// Alignment must be a power of 2
		assert((Alignment & (Alignment - 1)) == 0);
#endif
	}

	void* Malloc()
	{
		if (!mFirstBlocks32)
		{
			mFirstBlocks32 = AllocateBlocks32();
			mLastFreedBlocks32 = mFirstBlocks32;
		}
		size_t freeIndex = mLastFreedBlocks32->GetHighestFreeBit();
		if (freeIndex == 32)
		{
			// If no free block , we allocate new block
			if (!mLastFreedBlocks32->Next)
			{
				mLastFreedBlocks32->Next = AllocateBlocks32();
			}
			mLastFreedBlocks32 = mLastFreedBlocks32->Next;
			freeIndex = 31;
			HLVM_CONSTEXPR_ASSERT(bValidate, mLastFreedBlocks32->GetHighestFreeBit() == 31);
		}
		// Mask allocated index
		mLastFreedBlocks32->FreeBits &= ~(1u << freeIndex);
		// Setup block head
		FSmallBinnedBlockHead* BlockHead = R_C(FSmallBinnedBlockHead*, R_C(TBYTE*, mLastFreedBlocks32->P) + freeIndex * (Alignment + sizeof(FSmallBinnedBlockHead)));
		BlockHead->Pos = S_C(uint8_t, freeIndex);
		BlockHead->Alignment = Alignment;
		// Return actual pointer address
		return R_C(TBYTE*, BlockHead) + sizeof(FSmallBinnedBlockHead);
	}

	void Free(void* p) noexcept
	{
		FSmallBinnedBlockHead* BlockHead = R_C(FSmallBinnedBlockHead*, R_C(TBYTE*, p) - sizeof(FSmallBinnedBlockHead));
		{
			HLVM_CONSTEXPR_ASSERT(bValidate, BlockHead->Alignment == Alignment);
			bool bFound = false;
			auto Block = mFirstBlocks32;
			while (Block)
			{
				if (Block->Owned(p))
				{
					HLVM_CONSTEXPR_ASSERT(bValidate, Block->CheckFree(p));
					bFound = true;
					mLastFreedBlocks32 = Block;
					break;
				}
				Block = Block->Next;
			}
			HLVM_CONSTEXPR_ASSERT(bValidate, bFound);
		}
		// Mask free bit
		mLastFreedBlocks32->FreeBits |= 1u << BlockHead->Pos;
	}

private:
	FBlocks32* AllocateBlocks32();

	FBlocks32* mFirstBlocks32{ nullptr };
	FBlocks32* mLastFreedBlocks32{ nullptr }; // Cache last freed block to faster malloc next time
	FVMArena*  Mallocator{ nullptr };
	uint8_t	   Alignment{ 0 };
};
