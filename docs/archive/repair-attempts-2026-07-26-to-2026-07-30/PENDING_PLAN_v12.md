# Pending Plan v12 — make the v11 cerr-patch default-ON (drop the HLVM_FORCE_CERR_LOGGING macro gate) so the next parent rebuild produces a guaranteed-visible diagnostic signal in stderr regardless of spdlog behavior

- task: convert the v11 cerr-patch from macro-gated (dormant by default) to default-ON. The v11 patch added 2 cerr-write blocks guarded by `#ifdef HLVM_FORCE_CERR_LOGGING`. v12 removes the guards so the cerr writes fire unconditionally. This maximizes signal on the next parent rebuild.
- source: docs/PENDING_PLAN_v11.md (the v10a cerr-patch that landed) + the on-disk log evidence in `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log` (2026-07-27 00:07 run, gi_raw=0, command-list warning every frame, zero v3 spdlog markers — all evidence source/binary mismatch + spdlog-level-filter hypothesis)
- approach: 2 surgical edits, no behavior change to the dispatch pipeline, no other source touched. Drop the `#ifdef HLVM_FORCE_CERR_LOGGING` and matching `#endif` lines around the cerr block in each file. Keep the cerr writes themselves unchanged. Keep the `<iostream>` includes (now load-bearing, not dormant-conditional).

## Why this is the right v12 cycle (decision matrix execution from v9 evidence)

The pipeline's running hypothesis is **source/binary mismatch** + **possible spdlog-level-filter**. v9 evidence (the parent's actual log on disk):

1. `gi_raw = R[0,0] G[0,0] B[0,0]` (line 76 of TestReSTIR_GI_Temporal.log) — GI dispatch produces nothing
2. `"A command list should be executed before it is reopened"` fires every frame (lines 64-72) — the bug-088 issue is still present even after v5's HLVM-bypass removal
3. ZERO v3 spdlog markers in the log (Pre-GIPass, Post-GIPass, FGIPass::DispatchRays ENTER/EXIT, per-frame binding set) even though the source HAS them at TestReSTIR_GI_Temporal.cpp:435/442 and FGIPass.cpp:466/569/578
4. v11 cerr-patch is in source but DORMANT (macro-gated). On a default rebuild, it does nothing.

Two root-cause hypotheses compete:
- **H-A (source/binary mismatch)**: the binary on disk was built before v3 instrumentation was added. v3 logs not firing because the binary is stale. A clean rebuild will surface v3 logs.
- **H-B (spdlog-level-filter)**: the binary was rebuilt with v3 logs, but spdlog is silently filtering info-level messages from LogGI/LogTest. A clean rebuild will still not surface v3 logs.

v11's cerr-patch (dormant) does not help distinguish A vs B without the macro being defined. v12 makes the cerr default-ON. **If H-A is true**, after rebuild: v3 spdlog markers WILL fire + cerr will fire + gi_raw will likely still be 0 (separate bug) — this gives parent 3 signals, very informative. **If H-B is true**, after rebuild: v3 spdlog markers will NOT fire + cerr WILL fire + gi_raw will likely still be 0 — this proves spdlog-level-filter is the bug, and v12e (spdlog config fix) becomes the next step.

Either way, v12 converts the next rebuild from "diagnostic surface may or may not appear" into "cerr is guaranteed to fire." This is the maximally-informative file-only patch available.

## The patch

**File 1: `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp`**

Current state (verified via read_file at offset 455-469):
```cpp
    void FGIPass::DispatchRays(nvrhi::ICommandList* CmdList, const FGIPassDesc& Desc)
    {
#ifdef HLVM_FORCE_CERR_LOGGING
        // v11 patch (six-role-pipeline): unconditional stderr write that bypasses
        // spdlog level-filter. Dormant unless -DHLVM_FORCE_CERR_LOGGING is set.
        // Purpose: if spdlog is silently filtering LogGI info-level messages,
        // this still proves DispatchRays() body is entered.
        std::cerr << "[RGI] FGIPass::DispatchRays() entry: "
                  << "bIsInitialized=" << (bIsInitialized ? "true" : "false")
                  << " RTPipeline.Initialized=" << (RTPipeline.IsInitialized() ? "true" : "false")
                  << " SceneTLAS=0x" << std::hex << (uintptr_t)Desc.SceneTLAS.Get() << std::dec
                  << " OutputTex=0x" << (uintptr_t)Desc.OutputTexture.Get()
                  << " Frame=" << Desc.FrameIndex
                  << std::endl;
#endif
```

Replace with (remove `#ifdef` and `#endif`):
```cpp
    void FGIPass::DispatchRays(nvrhi::ICommandList* CmdList, const FGIPassDesc& Desc)
    {
        // v12 patch (six-role-pipeline): default-ON cerr write that bypasses
        // spdlog level-filter. v11 made this macro-gated; v12 un-gates it so
        // the next rebuild produces visible diagnostic in stderr regardless
        // of any spdlog configuration. Distinguishes H-A (source/binary
        // mismatch) from H-B (spdlog-level-filter) on the next parent run.
        std::cerr << "[RGI] FGIPass::DispatchRays() entry: "
                  << "bIsInitialized=" << (bIsInitialized ? "true" : "false")
                  << " RTPipeline.Initialized=" << (RTPipeline.IsInitialized() ? "true" : "false")
                  << " SceneTLAS=0x" << std::hex << (uintptr_t)Desc.SceneTLAS.Get() << std::dec
                  << " OutputTex=0x" << (uintptr_t)Desc.OutputTexture.Get()
                  << " Frame=" << Desc.FrameIndex
                  << std::endl;
```

Net: -2 lines (the `#ifdef` and `#endif`).

**File 2: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`**

Current state (verified via read_file at offset 377-388):
```cpp
    virtual void Render(nvrhi::IFramebuffer* Framebuffer) override
    {
#ifdef HLVM_FORCE_CERR_LOGGING
        // v11 patch (six-role-pipeline): unconditional stderr write that bypasses
        // spdlog level-filter. Dormant unless -DHLVM_FORCE_CERR_LOGGING is set.
        // Purpose: if the binary was rebuilt WITHOUT v3's instrumentation firing
        // (e.g., spdlog filter, level cutoff), this still proves Render() is reached.
        std::cerr << "[RGI] Render() entry: Frame=" << AccumFrameCount
                  << " CmdList=0x" << std::hex << (uintptr_t)CommandList.Get() << std::dec
                  << " NvrhiDevice=0x" << (uintptr_t)NvrhiDevice.Get()
                  << std::endl;
#endif
```

Replace with (remove `#ifdef` and `#endif`):
```cpp
    virtual void Render(nvrhi::IFramebuffer* Framebuffer) override
    {
        // v12 patch (six-role-pipeline): default-ON cerr write that bypasses
        // spdlog level-filter. v11 made this macro-gated; v12 un-gates it so
        // the next rebuild produces visible diagnostic in stderr regardless
        // of any spdlog configuration. Distinguishes H-A (source/binary
        // mismatch) from H-B (spdlog-level-filter) on the next parent run.
        std::cerr << "[RGI] Render() entry: Frame=" << AccumFrameCount
                  << " CmdList=0x" << std::hex << (uintptr_t)CommandList.Get() << std::dec
                  << " NvrhiDevice=0x" << (uintptr_t)NvrhiDevice.Get()
                  << std::endl;
```

Net: -2 lines (the `#ifdef` and `#endif`).

The `<iostream>` includes added in v11 become load-bearing (no longer conditional) but require no further change.

## diff_estimate

- `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp`: -2 / -0 (remove `#ifdef` and `#endif` lines)
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`: -2 / -0 (remove `#ifdef` and `#endif` lines)
- **Total: -4 / -0 lines, 0 lines of behavior change to the dispatch pipeline**

## skip_plan_review

no — patch changes source semantics (cerr writes now unconditional) and removes an opt-in mechanism. Plan-criticer must sign off on (a) the default-ON policy (some maintainers prefer opt-in; v12 explicitly rejects opt-in for the diagnostic-phase purpose), (b) the stderr output volume (8 frames × 2 lines/frame = 16 cerr lines per run — negligible, not log spam), (c) reversibility (adding the `#ifdef`/`#endif` back restores v11 byte-for-byte).

## test_strategy

No new test files needed. The patch is observable only on the next parent rebuild. The validator (`validate_restir_gi.py`) continues to apply unchanged against post-rebuild dumps.

### Parent-driven tests (terminal blocked in cron):

1. **Build cleanliness** (default rebuild, no macro): `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal`. Expected: clean build, no compile errors. cerr writes now unconditional; `<iostream>` includes from v11 are load-bearing.

2. **Run with default env vars**: `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log`. Expected:
   - stderr.log contains 8 lines starting with `[RGI] Render() entry:` and 8 lines starting with `[RGI] FGIPass::DispatchRays() entry:`
   - Each line has non-zero handles for CmdList, NvrhiDevice, SceneTLAS, OutputTex
   - TestReSTIR_GI_Temporal.log contains the v3 spdlog markers per frame IF H-A is true (binary was stale)
   - TestReSTIR_GI_Temporal.log MISSING the v3 spdlog markers IF H-B is true (spdlog filter)
   - gi_raw normalized per-channel log line — R[0,0] G[0,0] B[0,0] (likely still 0; separate bug)

3. **Vision-analyze `display_frame8.png`** for recognizable non-uniform Sponza geometry.

4. **Run validator**: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`. Expected: same 3-check validator as before. If v12's default-ON cerr + fresh rebuild surfaces a fix, validator should pass; if not, the same failure pattern as v11.

## risks

- **stderr output is now unconditional** in the rebuilt binary. Two cerr lines per frame = 16 lines per 8-frame run. Each line is ~150 bytes. Negligible disk/log impact; not a production concern because TestReSTIR_GI_Temporal is a test executable, not a shipped product. The cerr writes can be removed entirely once the underlying bug is found and fixed.
- **If the binary is rebuilt AND v3 spdlog markers NOW fire AND cerr also fires AND gi_raw is still 0**: v12 succeeded in distinguishing A vs B (H-A confirmed). Next step: investigate the gi_raw=0 root cause separately (the GI dispatch is running but producing nothing — bug is in dispatch body, not diagnostic path). v12a = investigate dispatch body.
- **If the binary is rebuilt AND v3 spdlog markers STILL don't fire AND cerr fires**: v12 succeeded in distinguishing A vs B (H-B confirmed). Next step: spdlog configuration fix (v12e = adjust `GLogConfig` or `FSpdlogConsoleDevice` to lower the LogGI/LogTest info-level cutoff).
- **If both fire and gi_raw is non-zero and display is correct**: pipeline complete (v6d). v12 succeeded beyond its diagnostic purpose; revert cerr writes in a follow-up cycle.
- **If parent cannot rebuild**: structural block persists, v12's patch is dormant in source but harmless. The next cron tick records this and exits with honest report.
- **If `<iostream>` was the only thing in v11 and v12 un-gates the cerr writes, the include is fine — std::cerr is a standard library symbol always available via `<iostream>`.**

## files

This cycle:
- `docs/PENDING_PLAN_v12.md` (this file)
- `docs/PENDING_PLAN_REVIEW_v12.md` (plan-critique)
- `docs/PENDING_COMMIT_v12.md` (impl summary: 2 lines removed in each of 2 files)
- `docs/PENDING_IMPL_REVIEW_v12.md`
- `docs/PENDING_TESTS_v12.md`
- `docs/PENDING_TEST_AUDIT_v12.md`
- `docs/PIPELINE_HEALTH_2026-07-27.md` (append this tick's section)
- `docs/PENDING_PICK.md` (mark v12 [x], keep v6a-2/v6a-d/v11b/c/d/v12a/e decision matrix as next-step options)

Source files modified:
- `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` (-2 lines: remove `#ifdef` and `#endif`)
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` (-2 lines: remove `#ifdef` and `#endif`)

## What parent must do (priority-ordered)

1. **Rebuild WITHOUT the macro** (default; this is v12's whole point):
   - `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
   - Run: `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log`
   - Inspect: `cat stderr.log` — expect 16 cerr lines (8 Render + 8 FGIPass::DispatchRays)
   - Inspect: `TestReSTIR_GI_Temporal.log` — expect v3 spdlog markers per frame IF H-A is true
   - Vision-analyze: `display_frame8.png`
   - Run validator: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`

2. **Report v12 evidence back to cron** with one of:
   - "cerr fires + v3 spdlog markers NOW fire + gi_raw still 0" → H-A confirmed. Cron routes to v12a (investigate dispatch body separately).
   - "cerr fires + v3 spdlog markers STILL don't fire + gi_raw still 0" → H-B confirmed. Cron routes to v12e (spdlog config fix).
   - "cerr fires + v3 spdlog markers NOW fire + gi_raw non-zero + display correct + validator 3/3" → pipeline complete (v6d). Revert cerr writes in a follow-up cycle.
   - "cerr does NOT fire after confirmed rebuild" → unexpected; v12c investigates (the binary's Render() / DispatchRays() is not being called at all — investigate control flow upstream of these functions).

## v12a/e decision matrix (post-rebuild evidence)

| Parent's answer | Next cycle |
|---|---|
| cerr fires + v3 spdlog NOW fire + gi_raw still 0 | v12a: investigate GI dispatch body (binding layout, payload layout, dispatch call) |
| cerr fires + v3 spdlog STILL don't fire + gi_raw still 0 | v12e: spdlog config fix (level filter, category cutoff in GLogConfig or FSpdlogConsoleDevice) |
| cerr fires + v3 spdlog NOW fire + gi_raw non-zero + display correct + validator 3/3 | **pipeline complete (v6d)** — revert cerr writes in follow-up |
| cerr does NOT fire | v12c: cerr is not reaching stderr (investigate stderr buffering or output capture) |
| Parent cannot rebuild | structural block persists; cron records honestly |

## Honesty caveats

- All 6 roles are the same head (single-profile, single-prompt host). KEEP verdicts are self-checks.
- This patch changes source semantics (cerr writes default-ON). The v11 macro-gated behavior is gone. Reversible by adding the `#ifdef`/`#endif` lines back.
- The hypothesis (source/binary mismatch vs spdlog-level-filter) is the most likely root cause as of v12. v12's patch is diagnostic surface, not corrective.
- The patch is purely subtractive on the macro guards, additive on the unconditional cerr behavior. Behavior change is minimal (8 frames × 2 cerr lines/run = 16 lines/8-frame run, all to stderr).
