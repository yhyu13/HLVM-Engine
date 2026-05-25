/**
 * FGPUProfiler.cpp
 * Production GPU frame profiler using NVRHI timer queries.
 */

#include "Renderer/Utility/FGPUProfiler.h"
#include "Core/Log.h"

#include <imgui.h>

DECLARE_LOG_CATEGORY(LogProfiler)

void FGPUProfiler::Initialize(nvrhi::IDevice* InDevice)
{
    Shutdown();
    Device = InDevice;
    bInitialized = true;
    HLVM_LOG(LogProfiler, info, TXT("FGPUProfiler initialized (ring depth={})"), RingBufferDepth);
}

void FGPUProfiler::Shutdown()
{
    if (!bInitialized)
    {
        return;
    }

    // Release all queries from all ring buffer slots
    for (uint32_t i = 0; i < RingBufferDepth; ++i)
    {
        for (auto& Timer : RingBuffer[i])
        {
            Timer.Query = nullptr;
        }
        RingBuffer[i].clear();
    }

    for (auto& Timer : CurrentFrameQueries)
    {
        Timer.Query = nullptr;
    }
    CurrentFrameQueries.clear();

    // Drop query pool references — NVRHI will reclaim them
    QueryPool.clear();

    LastFrameTimings.clear();
    SmoothedTimings.clear();
    Device = nullptr;
    bInitialized = false;
}

nvrhi::TimerQueryHandle FGPUProfiler::AcquireQuery()
{
    if (!Device)
    {
        return nullptr;
    }

    // Try to recycle from pool
    while (!QueryPool.empty())
    {
        nvrhi::TimerQueryHandle Query = QueryPool.back();
        QueryPool.pop_back();
        if (Query)
        {
            Device->resetTimerQuery(Query);
            return Query;
        }
    }

    // Pool empty — allocate new
    return Device->createTimerQuery();
}

void FGPUProfiler::ReleaseQuery(nvrhi::TimerQueryHandle Query)
{
    if (Query && Device)
    {
        Device->resetTimerQuery(Query);
        QueryPool.push_back(Query);
    }
}

void FGPUProfiler::BeginFrame()
{
    if (!bInitialized || !bEnabled)
    {
        return;
    }

    // Advance ring buffer index
    CurrentFrameIndex = (CurrentFrameIndex + 1) % RingBufferDepth;

    // Resolve the OLDEST frame's queries (now 3 frames behind)
    TVector<FPassTimer>& OldestSlot = RingBuffer[CurrentFrameIndex];

    LastFrameTimings.clear();

    for (auto& Timer : OldestSlot)
    {
        if (!Timer.Query)
        {
            continue;
        }

        if (Timer.bResolved)
        {
            // Already resolved in a previous attempt — just release
            ReleaseQuery(Timer.Query);
            Timer.Query = nullptr;
            continue;
        }

        if (Device->pollTimerQuery(Timer.Query))
        {
            float DeltaMs = Device->getTimerQueryTime(Timer.Query);
            Timer.bResolved = true;

            // Update EMA smoothing
            float& Smoothed = SmoothedTimings[Timer.Name];
            if (Smoothed == 0.0f)
            {
                // First sample — initialize directly
                Smoothed = DeltaMs;
            }
            else
            {
                Smoothed = EMA_Alpha * DeltaMs + (1.0f - EMA_Alpha) * Smoothed;
            }

            LastFrameTimings[Timer.Name] = Smoothed;

            // Release back to pool
            ReleaseQuery(Timer.Query);
            Timer.Query = nullptr;
        }
        // else: query not ready yet — leave in slot, will retry next frame
    }

    // Compact slot: remove resolved/released entries
    auto It = OldestSlot.begin();
    while (It != OldestSlot.end())
    {
        if (!It->Query)
        {
            It = OldestSlot.erase(It);
        }
        else
        {
            ++It;
        }
    }

    // Release any remaining unresolved queries — they had 3 frames to resolve.
    // Keeping them would cause the slot to grow unbounded in EndFrame().
    for (auto& Timer : OldestSlot)
    {
        if (Timer.Query)
        {
            ReleaseQuery(Timer.Query);
            Timer.Query = nullptr;
        }
    }
    OldestSlot.clear();
}

void FGPUProfiler::BeginPass(nvrhi::ICommandList* CmdList, const FString& Name)
{
    if (!bInitialized || !bEnabled || !CmdList || !Device)
    {
        return;
    }

    nvrhi::TimerQueryHandle Query = AcquireQuery();
    if (!Query)
    {
        HLVM_LOG(LogProfiler, warn, TXT("FGPUProfiler: Failed to acquire timer query"));
        // Push a dummy entry so EndPass() pairs correctly
        // and doesn't end the previous pass's query again
        CurrentFrameQueries.push_back({nullptr, Name, false});
        return;
    }

    CmdList->beginTimerQuery(Query);

    FPassTimer Timer;
    Timer.Query = Query;
    Timer.Name = Name;
    Timer.bResolved = false;
    CurrentFrameQueries.push_back(Timer);
}

void FGPUProfiler::EndPass(nvrhi::ICommandList* CmdList)
{
    if (!bInitialized || !bEnabled || !CmdList || !Device)
    {
        return;
    }

    if (!CurrentFrameQueries.empty() && CurrentFrameQueries.back().Query)
    {
        CmdList->endTimerQuery(CurrentFrameQueries.back().Query);
    }
}

void FGPUProfiler::EndFrame()
{
    if (!bInitialized || !bEnabled || !Device)
    {
        return;
    }

    // Move current frame queries into the ring buffer slot
    // (this slot was just resolved in BeginFrame, or is empty on first frames)
    TVector<FPassTimer>& Slot = RingBuffer[CurrentFrameIndex];

    for (auto& Timer : CurrentFrameQueries)
    {
        Slot.push_back(Timer);
    }
    CurrentFrameQueries.clear();
}

float FGPUProfiler::GetTotalFrameTime() const
{
    float Total = 0.0f;
    for (const auto& [Name, Ms] : LastFrameTimings)
    {
        Total += Ms;
    }
    return Total;
}

void FGPUProfiler::DrawUI()
{
    if (!bEnabled || !bShowUI)
    {
        return;
    }

    if (!ImGui::Begin("GPU Profiler", &bShowUI))
    {
        ImGui::End();
        return;
    }

    ImGui::Text("Frame Time: %.3f ms (%.1f FPS)", static_cast<double>(GetTotalFrameTime()),
                static_cast<double>(GetTotalFrameTime() > 0.0f ? 1000.0f / GetTotalFrameTime() : 0.0f));
    ImGui::Separator();

    if (LastFrameTimings.empty())
    {
        ImGui::TextUnformatted(CHARSTR("No data yet -- wait for ring buffer to fill..."));
    }
    else
    {
        // Bar chart
        float MaxTime = 0.0f;
        for (const auto& [Name, Ms] : LastFrameTimings)
        {
            MaxTime = std::max(MaxTime, Ms);
        }

        if (MaxTime <= 0.0f)
        {
            MaxTime = 1.0f;
        }

        ImGui::TextUnformatted(CHARSTR("Pass Breakdown:"));
        for (const auto& [Name, Ms] : LastFrameTimings)
        {
            float Fraction = Ms / MaxTime;
            ImGui::ProgressBar(Fraction, ImVec2(-FLT_MIN, 0.0f),
                               CHARSTR(FString::Format(TXT("{}: {:.3f} ms"), *Name, Ms).c_str()));
        }

        ImGui::Separator();

        // Table
        if (ImGui::BeginTable("ProfilerTable", 3,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Pass");
            ImGui::TableSetupColumn("Time (ms)");
            ImGui::TableSetupColumn("%% of Frame");
            ImGui::TableHeadersRow();

            float Total = GetTotalFrameTime();
            for (const auto& [Name, Ms] : LastFrameTimings)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(CHARSTR(Name.c_str()));
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.4f", static_cast<double>(Ms));
                ImGui::TableSetColumnIndex(2);
                float Pct = Total > 0.0f ? (Ms / Total) * 100.0f : 0.0f;
                ImGui::Text("%.1f%%", static_cast<double>(Pct));
            }

            ImGui::EndTable();
        }
    }

    ImGui::End();
}
