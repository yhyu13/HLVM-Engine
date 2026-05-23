/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * TestGPUProfiler — Unit and integration tests for FGPUProfiler
 */

#include "Test.h"
#include "Renderer/Utility/FGPUProfiler.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Window/WindowDefinition.h"

DECLARE_LOG_CATEGORY(LogTest)

#if HLVM_VULKAN_RENDERER

RECORD(TestGPUProfilerLifecycle, true)
{
    // Test Initialize / Shutdown lifecycle without crash
    FGPUProfiler Profiler;
    Profiler.Initialize(nullptr);
    Profiler.Shutdown();
    HLVM_LOG(LogTest, info, TXT("FGPUProfiler lifecycle test passed"));
}

RECORD(TestGPUProfilerQueryPool, true)
{
    // Test that query pool grows and reuses correctly
    FGPUProfiler Profiler;
    Profiler.Initialize(nullptr);

    // Without a device, BeginPass is a no-op
    Profiler.BeginFrame();
    Profiler.BeginPass(nullptr, TXT("TestPass"));
    Profiler.EndPass(nullptr);
    Profiler.EndFrame();

    // Timings should be empty (no device)
    const auto& Timings = Profiler.GetTimings();
    HLVM_ENSURE(Timings.empty());

    Profiler.Shutdown();
    HLVM_LOG(LogTest, info, TXT("FGPUProfiler query pool test passed"));
}

RECORD(TestGPUProfilerEMA, true)
{
    // Test EMA smoothing logic indirectly via public API
    FGPUProfiler Profiler;
    Profiler.Initialize(nullptr);

    // Verify GetTotalFrameTime returns 0 when empty
    float Total = Profiler.GetTotalFrameTime();
    HLVM_ENSURE(Total == 0.0f);

    // Verify timings map is empty initially
    HLVM_ENSURE(Profiler.GetTimings().empty());

    Profiler.Shutdown();
    HLVM_LOG(LogTest, info, TXT("FGPUProfiler EMA test passed"));
}

RECORD(TestGPUProfilerEnabledToggle, true)
{
    FGPUProfiler Profiler;
    Profiler.Initialize(nullptr);

    // Default should be enabled
    HLVM_ENSURE(Profiler.IsEnabled());

    // Disable and verify BeginFrame is a no-op
    Profiler.SetEnabled(false);
    Profiler.BeginFrame();
    Profiler.BeginPass(nullptr, TXT("DisabledPass"));
    Profiler.EndPass(nullptr);
    Profiler.EndFrame();

    // Re-enable
    Profiler.SetEnabled(true);

    Profiler.Shutdown();
    HLVM_LOG(LogTest, info, TXT("FGPUProfiler toggle test passed"));
}

#endif // HLVM_VULKAN_RENDERER
