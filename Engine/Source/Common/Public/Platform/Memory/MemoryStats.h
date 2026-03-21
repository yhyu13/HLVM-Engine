/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"

/**
 * Contains information about current memory usage and capacity.
 */
PACK(struct FMemoryStatsCPU {
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
});

/**
 * The GPU memory stats.
 */
PACK(struct FMemoryStatsGPU {
	/**
	 * The total amount of memory in bytes (as reported by the driver).
	 */
	TUINT64 Total;

	/**
	 * The used by the game amount of memory in bytes (estimated).
	 */
	TUINT64 Used;
});
