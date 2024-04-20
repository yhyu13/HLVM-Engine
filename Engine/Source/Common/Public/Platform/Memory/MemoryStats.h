/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

/**
 * Contains information about current memory usage and capacity.
 */
struct FMemoryStats
{
	/**
	 * Total amount of physical memory in bytes.
	 */
	TUINT64 TotalPhysicalMemory;

	/**
	 * Amount of used physical memory in bytes.
	 */
	TUINT64 UsedPhysicalMemory;

	/**
	 * Total amount of virtual memory in bytes.
	 */
	TUINT64 TotalVirtualMemory;

	/**
	 * Amount of used virtual memory in bytes.
	 */
	TUINT64 UsedVirtualMemory;
};

/**
 * Contains information about current memory usage by the process.
 */
struct FProcessMemoryStats
{
	/**
	 * Amount of used physical memory in bytes.
	 */
	TUINT64 UsedPhysicalMemory;

	/**
	 * Amount of used virtual memory in bytes.
	 */
	TUINT64 UsedVirtualMemory;
};
