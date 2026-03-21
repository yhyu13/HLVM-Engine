/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "PackedToken.h"
#include "Platform/PlatformDefinition.h"

#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

struct FPackedTokenEntryDataAndFragmentID
{
	FPackedTokenEntryData Data;
	uint32_t			  FragmentID;
};

/**
 * Mmap a fragment of the container file (<4MB files are concated to less than 4MB, >4MB file stand alone one fragment)
 * So that we can call less mmap and less page fault;
 * And since there is a ref counter, we need to make sure struct is cache aligned to avoid false sharing
 */
struct FPackedContainerFragment
{
	static size_t sSuggestedFragmentSize;

	std::atomic_uint_fast16_t				 FramgentRefCount{ 0 };
	boost::interprocess::mapped_region		 Region;
	const boost::interprocess::file_mapping* ContainerFileMapping;
	size_t									 FragmentStartPos{ 0 };
	size_t									 FragmentSize{ 0 };

	NOCOPY(FPackedContainerFragment);
	FPackedContainerFragment() = default;
	FPackedContainerFragment(FPackedContainerFragment&& Other)
	{
		FramgentRefCount = Other.FramgentRefCount.load(std::memory_order_relaxed);
		Region = MoveTemp(Other.Region);
		ContainerFileMapping = MoveTemp(Other.ContainerFileMapping);
		FragmentStartPos = MoveTemp(Other.FragmentStartPos);
		FragmentSize = MoveTemp(Other.FragmentSize);
	}

	void							Open();
	void							Close();
	HLVM_NODISCARD FConstByteBuffer GetSubRegion(const FPackedTokenEntryData& Data) const;
};

struct FPackedEntryQuickFind
{
	const FPackedTokenEntryData* Data{ nullptr };
	FPackedContainerFragment*	 Fragment{ nullptr };
	std::atomic_uint_fast32_t*	 RefCount{ nullptr };
};
