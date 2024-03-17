/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Mallocator/VMMallocator/SmallBinnedMallocator.h"
#include "Core/Mallocator/VMMallocator/VMArena.h"

FSmallBinnedMallocator::~FSmallBinnedMallocator() noexcept
{
	while (mFirstBlocks32)
	{
		Mallocator->Free(mFirstBlocks32->P);
		FBlocks32* Next = mFirstBlocks32->Next;
		Mallocator->Free(mFirstBlocks32);
		mFirstBlocks32 = Next;
	}
}

FBlocks32* FSmallBinnedMallocator::AllocateBlocks32()
{
	FBlocks32* Temp = R_C(FBlocks32*, Mallocator->Malloc(sizeof(FBlocks32)));
	*Temp = FBlocks32();
	Temp->P = Mallocator->Malloc(((Alignment + sizeof(FSmallBinnedBlockHead)) << 5));
	Temp->Alignment = Alignment;
	return Temp;
}
