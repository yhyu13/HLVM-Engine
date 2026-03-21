/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"

#ifndef HLVM_COMPILE_WITH_RENDER_STATS
	#define HLVM_COMPILE_WITH_RENDER_STATS !HLVM_SHIPPING_CODE
#endif

#include <atomic>

/**
 * Object that stores various render statistics.
 */
struct FRenderStatsData
{
	/**
	 * The draw calls count.
	 */
	std::atomic_uint32_t DrawCalls;

	/**
	 * The compute shader dispatch calls count.
	 */
	std::atomic_uint32_t DispatchCalls;

	/**
	 * The pipeline state changes count.
	 */
	std::atomic_uint32_t PipelineStateChanges;

	/**
	 * The vertices drawn count.
	 */
	std::atomic_uint32_t Vertices;

	/**
	 * The triangles drawn count.
	 */
	std::atomic_uint32_t Triangles;

	/**
	 * Initializes a new instance of the FRenderStatsData struct.
	 */
	FRenderStatsData()
		: DrawCalls(0)
		, DispatchCalls(0)
		, PipelineStateChanges(0)
		, Vertices(0)
		, Triangles(0)
	{
	}

	/**
	 * Mixes the stats with the current state. Perform operation: this  = currentState - this, but without additional stack allocations.
	 *
	 * @param currentState The current state.
	 */
	void Update(const FRenderStatsData& currentState)
	{
		DrawCalls = currentState.DrawCalls - DrawCalls;
		DispatchCalls = currentState.DispatchCalls - DispatchCalls;
		PipelineStateChanges = currentState.PipelineStateChanges - PipelineStateChanges;
		Vertices = currentState.Vertices - Vertices;
		Triangles = currentState.Triangles - Triangles;
	}
};

HLVM_INLINE_FUNC HLVM_STATIC_VAR FRenderStatsData GRenderStatData{};

#if HLVM_COMPILE_WITH_RENDER_STATS
	#define HLVM_RENDER_STAT_DISPATCH_CALL() (GRenderStatData.DispatchCalls).fetch_add(1, std::memory_order_relaxed)
	#define HLVM_RENDER_STAT_PS_STATE_CHANGE() (GRenderStatData.PipelineStateChanges).fetch_add(1, std::memory_order_relaxed)
	#define HLVM_RENDER_STAT_DRAW_CALL(vertices, triangles)                              \
		do                                                                               \
		{                                                                                \
			(GRenderStatData.DrawCalls).fetch_add(1, std::memory_order_relaxed);         \
			(GRenderStatData).Vertices.fetch_add(vertices, std::memory_order_relaxed);   \
			(GRenderStatData).Triangles.fetch_add(triangles, std::memory_order_relaxed); \
		}                                                                                \
		while (0)
#else
	#define HLVM_RENDER_STAT_DISPATCH_CALL()
	#define HLVM_RENDER_STAT_PS_STATE_CHANGE()
	#define HLVM_RENDER_STAT_DRAW_CALL(vertices, primitives)
#endif
