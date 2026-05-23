#pragma once

#include "Core/String.h"
#include "Core/Container/ContainerDefinition.h"

#include <nvrhi/nvrhi.h>

// Forward declare ImGui types to avoid forcing imgui.h on all consumers
struct ImDrawList;

/**
 * @brief Production GPU frame profiler using NVRHI timer queries
 *
 * Measures per-pass GPU time with:
 * - 3-frame ring buffer for pipelined rendering support
 * - Query pool reuse to eliminate per-frame allocation churn
 * - Automatic retry for queries not yet ready
 * - Exponential moving average smoothing
 * - Optional ImGui overlay (DrawUI)
 *
 * Usage:
 *   FGPUProfiler Profiler;
 *   Profiler.Initialize(Device);
 *
 *   // Each frame:
 *   Profiler.BeginFrame();                 // NEW: call before first BeginPass
 *   Profiler.BeginPass(CmdList, TXT("GBuffer"));
 *   // ... render GBuffer ...
 *   Profiler.EndPass(CmdList);
 *   // ... more passes ...
 *   Profiler.EndFrame();                   // After executeCommandList + waitForIdle
 *
 *   // Query timings:
 *   for (auto& [Name, Ms] : Profiler.GetTimings()) { ... }
 */
class FGPUProfiler
{
public:
    static constexpr uint32_t RingBufferDepth = 3;
    static constexpr float EMA_Alpha = 0.1f;

    FGPUProfiler() = default;
    ~FGPUProfiler() { Shutdown(); }

    void Initialize(nvrhi::IDevice* InDevice);
    void Shutdown();

    // Call once per frame before the first BeginPass.
    void BeginFrame();

    // Begin timing a pass. Call before the pass's rendering commands.
    void BeginPass(nvrhi::ICommandList* CmdList, const FString& Name);

    // End timing a pass. Call after the pass's rendering commands.
    void EndPass(nvrhi::ICommandList* CmdList);

    // Call once per frame after all GPU work is submitted and idle.
    // Collects results from the oldest ring-buffer frame.
    void EndFrame();

    // Returns smoothed timings from the most recently completed frame (name -> milliseconds)
    [[nodiscard]] const TMap<FString, float>& GetTimings() const { return LastFrameTimings; }

    // Total GPU frame time (sum of all pass timings)
    [[nodiscard]] float GetTotalFrameTime() const;

    // ImGui overlay. Call inside an active ImGui::NewFrame context.
    void DrawUI();

    // Enable/disable profiling (disabled profiler is a no-op)
    void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }
    [[nodiscard]] bool IsEnabled() const { return bEnabled; }

private:
    struct FPassTimer
    {
        nvrhi::TimerQueryHandle Query;
        FString Name;
        bool bResolved = false;
    };

    nvrhi::TimerQueryHandle AcquireQuery();
    void ReleaseQuery(nvrhi::TimerQueryHandle Query);

    nvrhi::IDevice* Device = nullptr;

    // Ring buffer: each slot holds one frame's queries
    TVector<FPassTimer> RingBuffer[RingBufferDepth];
    uint32_t CurrentFrameIndex = 0;

    // Queries being recorded for the current frame (not yet in ring buffer)
    TVector<FPassTimer> CurrentFrameQueries;

    // Pool of recycled queries to avoid allocation churn
    TVector<nvrhi::TimerQueryHandle> QueryPool;

    // Smoothed timings (EMA) per pass
    TMap<FString, float> LastFrameTimings;
    TMap<FString, float> SmoothedTimings;

    bool bInitialized = false;
    bool bEnabled = true;
    bool bShowUI = true;
};
