# Pending Plan v3

- task: Apply diagnostic HLVM_LOG instrumentation to capture GIPass dispatch state, then defer to parent for fresh-log capture (tirith blocks terminal in cron, so build+run is parent-driven).
- source: no bundle
- approach: Add 8 small HLVM_LOG info-level calls in two places that, when parent builds+reruns the binary with HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8, will definitively answer WHY gi_raw remains 0,0,0. No behavior change — pure logging instrumentation. Three questions the logs answer: (a) does GIPass.DispatchRays reach its body? (b) is the binding set created? (c) does RTPipeline.DispatchRays return? (d) is the OutputTexture handle non-null and what is its Vulkan-tracked layout at dispatch time? Files touched: `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` (around line 540) and `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` (around lines 399-435, 1505-1516).
- diff_estimate: +25 lines (10 HLVM_LOG + 15 lines surrounding helper context); -0 lines.
- skip_plan_review: no — diagnostic patch is small but consequential; review confirms the instrumentation correctly captures dispatch state.
- test_strategy: parent must (1) build with `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`, (2) run with `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8`, (3) capture the new log and paste the relevant lines, (4) optionally run the validator on the dumps.
- risks:
  - The diagnostic patch is INFORMATIONAL only — no behavior change, no risk of regression.
  - The root cause fix is gated on diagnostic data. Speculative patches based on file-only analysis are explicitly forbidden by the v1/v2 lessons.
  - This cycle's value is producing the diagnostic data needed for v4's fix.

## Why diagnostic logging is the right move

From the v1/v2 lessons (PENDING_TEST_AUDIT_v2):
- v1 added a speculative patch (end-of-Render close+executeCommandList) that was falsified by parent verification (gi_raw remained 0,0,0).
- v2 added a worse speculative patch (CommandList->open() after RenderGBuffer) that was reverted because it would error on already-open CL.

Both cycles wasted effort. The lesson: **without diagnostic data, any speculative fix has ~0% chance of landing right** and a non-trivial chance of making things worse.

The diagnostic logs in this plan will answer three specific questions:

### Q1: Does `GIPass.DispatchRays` reach its body?

Currently `GIPass::DispatchRays` returns early if `!bIsInitialized || !RTPipeline.IsInitialized()`. A new log at line 456 will confirm whether the early-return path fires.

### Q2: Is the binding set created successfully?

Currently the binding set is built at line 531 with SetBuilder, but a creation failure returns silently at line 537 with only an err log. A new log will tell us whether `BindingSet` is null.

### Q3: Does `RTPipeline.DispatchRays` actually return?

Currently the dispatch call at line 540 is silent. A pre-dispatch log + a post-dispatch log bracketing line 540 will tell us if the dispatch call returns (it always does, but if it doesn't return, that means a stack overflow or fatal).

### Q4: Is the OutputTexture layout correctly transitioned at dispatch time?

The `setTextureState(OutputTexture, UnorderedAccess)` at line 467 happens BEFORE `setRayTracingState` (line 285 of FRayTracingPipeline.cpp) which does `insertRayTracingResourceBarriers`. If the layout tracking is out of sync (e.g., the previous RenderGBuffer close+execute+waitForIdle reset nvrhi's tracked state without resetting Vulkan's), the dispatch may have transitioned the texture to a wrong layout. Adding a log that prints the OutputTexture handle pointer BEFORE `setTextureState` will let us correlate with the dump.

## Concrete diagnostic patch

### Patch 1: FGIPass.cpp lines 454-543 — bracket the dispatch with state logs

```cpp
void FGIPass::DispatchRays(nvrhi::ICommandList* CmdList, const FGIPassDesc& Desc)
{
    if (!bIsInitialized || !RTPipeline.IsInitialized())
    {
        HLVM_LOG(LogGI, warning, TXT("FGIPass::DispatchRays: EARLY-RETURN bIsInitialized={} RTPipeline.Initialized={}"),
            bIsInitialized, RTPipeline.IsInitialized());
        return;
    }

    if (!Desc.SceneTLAS || !Desc.OutputTexture || !Desc.ViewConstants)
    {
        HLVM_LOG(LogGI, err, TXT("FGIPass::DispatchRays: missing required handles"));
        return;
    }

    // DIAGNOSTIC (v3): log entry state
    HLVM_LOG(LogGI, info, TXT("FGIPass::DispatchRays ENTER: OutputTex=0x{:x} OutputW={} OutputH={} Frame={}"),
        (uintptr_t)Desc.OutputTexture.Get(),
        Desc.OutputWidth, Desc.OutputHeight, Desc.FrameIndex);

    WriteConstants(CmdList, Desc);
    // ... existing code unchanged through line 531 ...

    // DIAGNOSTIC (v3): log post-binding-set state
    if (!BindingSet)
    {
        HLVM_LOG(LogGI, err, TXT("FGIPass: failed to create per-frame binding set"));
        return;
    }
    HLVM_LOG(LogGI, info, TXT("FGIPass: per-frame binding set created OK (handle=0x{:x})"), (uintptr_t)BindingSet.Get());

    RTPipeline.DispatchRays(CmdList, Desc.OutputWidth, Desc.OutputHeight, 1, BindingSet);

    // DIAGNOSTIC (v3): log dispatch return
    HLVM_LOG(LogGI, info, TXT("FGIPass::DispatchRays EXIT: dispatch returned, OutputTex=0x{:x}"),
        (uintptr_t)Desc.OutputTexture.Get());

    OutputTexture = Desc.OutputTexture;
}
```

### Patch 2: TestReSTIR_GI_Temporal.cpp lines 399-435 — bracket GIPass.DispatchRays with logs

```cpp
// (1) GI ray trace — produces OutputTexture (HDR rgb + hitDist alpha)
{
    GI::FGIPassDesc Desc{};
    // ... existing setup ...

    HLVM_LOG(LogTest, info, TXT("Pre-GIPass: CommandList=0x{:x} OutputTex=0x{:x}"),
        (uintptr_t)CommandList.Get(),
        (uintptr_t)Desc.OutputTexture.Get());

    GIPass.DispatchRays(CommandList, Desc);

    HLVM_LOG(LogTest, info, TXT("Post-GIPass: returned"));
}
```

### Patch 3: TestReSTIR_GI_Temporal.cpp lines 1510-1515 — confirm the RenderGBuffer close+execute state

Add a single info-level log AFTER the waitForIdle at line 1512 confirming the queue went idle and the reopen at 1515 succeeded:

```cpp
CommandList->close();
NvrhiDevice->executeCommandList(CommandList);
NvrhiDevice->waitForIdle();

// DIAGNOSTIC (v3): confirm waitForIdle returned and queue is idle
HLVM_LOG(LogTest, info, TXT("RenderGBuffer: post-waitForIdle, queue idle; reopening CommandList"));

// Reopen for the remaining passes (FGIPass, denoise, ReSTIR, etc.).
CommandList->open();
```

## Acceptance criteria for v3

**Diagnostic-only cycle.** v3 is COMPLETE when:

1. The three diagnostic patches above are applied (no behavior change).
2. The parent runs `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`.
3. The parent runs `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` and captures the log.
4. The log shows: (a) GIPass::DispatchRays ENTER (not the early-return warning), (b) per-frame binding set created OK, (c) FGIPass::DispatchRays EXIT.
5. The log shows whether the `RenderGBuffer: post-waitForIdle, queue idle` line appears once per frame (it should — 8 times for ACCUM=8).
6. Optional: validator returns 3/3 PASS if the diagnostic data reveals the root cause is elsewhere and a follow-up patch fixes it; 0/3 FAIL otherwise (root cause is GIPass-specific).

The actual fix is NOT this cycle's deliverable. The fix is gated on the diagnostic data.