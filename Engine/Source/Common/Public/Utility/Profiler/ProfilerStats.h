/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

/**
 * Inspired by Flax Engine :
 * https://github.com/yhyu13/FlaxEngine/blob/b2f9da4113a7c80586ce3c0e7a916a40f0c63f04/Source/Engine/Profiler/
 * https://flaxengine.com/licensing
 */

#include "Platform/Memory/MemoryStats.h"
#include "Platform/Render/RenderStatsData.h"

#include "ProfilerCPU.h"
// TODO : YuHang GPU profiler
#include "ProfilerGPU.h"

/**
 * Profiler tools for development. Allows to gather profiling data and events from the engine.
 */
class FProfilerStats
{
	NOCOPYMOVE(FProfilerStats);

public:
	/**
	 * Engine profiling data header. Contains main info and stats.
	 */
	PACK(struct FMainStats {
		/**
		 * The CPU memory stats.
		 */
		FMemoryStatsCPU MemoryCPU;

		/**
		 * The GPU memory stats.
		 */
		FMemoryStatsGPU MemoryGPU;

		/**
		 * The frames per second (fps counter).
		 */
		TUINT16 FPS;

		/**
		 *  The update time on CPU (in microseconds).
		 */
		TUINT16 GameCPUTimeMicros;

		/**
		 * The update time on CPU (in microseconds).
		 */
		TUINT16 PhysicsCPUTimeMicros;

		/**
		 * The abstract render command generation time on CPU (in microseconds).
		 */
		TUINT16 DrawCmdCPUTimeMicros;

		/**
		 * The RHI time on CPU (in microseconds).
		 */
		TUINT16 DrawHardwareInterfaceCPUTimeMicros;

		/**
		 * The RHI time on GPU (in microseconds).
		 */
		TUINT16 DrawHardwareInterfaceGPUTimeMicros;

		/**
		 * The last rendered frame stats.
		 */
		FRenderStatsData DrawStats;
	});

	/**
	 * The CPU thread stats.
	 */
	struct FThreadStats
	{
		/**
		 * The thread name
		 */
		FString Name;

		/**
		 * The events list.
		 */
		TVector<FProfilerCPU::FTrackedEvent> CPUEvents;
	};

	/**
	 * The network stat.
	 */
	struct NetworkEventStat
	{
		// Amount of occurrences.
		TUINT16 Count;
		// Transferred data size (in bytes).
		TUINT16 DataSize;
		// Transferred message (data+header) size (in bytes).
		TUINT16 MessageSize;
		// Amount of peers that will receive this message.
		TUINT16 Receivers;

		TCharArray<63> Name;
	};

public:
	/*
	 * Controls the engine profiler (CPU, GPU, etc.) usage.
	 */
	static bool IsEnabled();

	/**
	 * Controls the engine profiler (CPU, GPU, etc.) usage.
	 * @param enabled - True if the profiler should be enabled.
	 */
	static void SetEnabled(bool enabled);

	/**
	 * The current collected main stats by the profiler from the local session. Updated every frame.
	 */
	static FMainStats Stats;

	/**
	 * The CPU threads profiler events.
	 */
	static TSmallVector64<std::unique_ptr<FThreadStats>> EventsCPUThread;

	/**
	 * The GPU rendering profiler events.
	 */
	static TVector<FProfilerGPU::FTrackedEvent> EventsGPU;

	/**
	 * The networking profiler events.
	 */
	static TVector<NetworkEventStat> EventsNetwork;
};
