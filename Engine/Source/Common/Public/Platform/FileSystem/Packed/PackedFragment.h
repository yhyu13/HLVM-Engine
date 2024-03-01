/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "PackedToken.h"

#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

struct FPackedTokenEntryDataAndFragmentID
{
	FPackedTokenEntryData Data;
	uint64_t			  FragmentID;
};

/**
 * Mmap a fragment of the container file (<4MB files are concated to less than 4MB, >4MB file stand alone one fragment)
 * So that we can call less mmap and less page fault;
 */
struct FPackedContainerFragment
{
	static size_t					   sSuggestedFragmentSize;
	boost::interprocess::mapped_region Region;
	size_t							   FragmentStartPos{ 0 };
	size_t							   FragmentSize{ 0 };

	void			 Open(const boost::interprocess::file_mapping& fm);
	void			 Close();
	FConstByteBuffer GetSubRegion(const FPackedTokenEntryData& Data) const;
};
