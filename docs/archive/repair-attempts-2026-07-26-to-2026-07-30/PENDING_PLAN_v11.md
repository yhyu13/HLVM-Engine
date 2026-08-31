# Pending Plan v11 — apply the v10a cerr-patch (dormant stderr writes gated by HLVM_FORCE_CERR_LOGGING) as the next mechanically actionable file-only fix

- task: apply the v10a cerr-patch (offered but not applied in v10) so that the next parent rebuild can produce a guaranteed-bypass diagnostic surface, independent of any spdlog level-filter theory. v11 is file-only; no terminal required.
- source: PENDING_PLAN_v10.md lines 47-57 + lines 91-94 (the staged cerr-patch sketch).
- approach: pure code patch, dormant by default. Two surgical inserts — one in TestReSTIR_GI_Temporal.cpp::Render() and one in FGIPass.cpp::DispatchRays() — each guarded by `#ifdef HLVM_FORCE_CERR_LOGGING` so the binary is byte-identical to v10 when the macro is undefined. When the macro IS defined (via `-DHLVM_FORCE_CERR_LOGGING` in Build.sh or CXXFLAGS), the cerr writes fire unconditionally and bypass any spdlog level filter.

## Why this is the right v11 cycle (v6a decision matrix execution)

Per the v9 evidence (still the freshest on-disk log evidence as of v11):

| Branch from PENDING_PICK v6 decision matrix | Match? |
|---|---|
| v5-fixed-everything (v6d) | FALSIFIED — gi_raw still 0, command-list warning still fires |
| gi_raw non-zero but validator < 3/3 (v6b) | FALSIFIED — gi_raw is 0 |
| validator 3/3 but display bad (v6c) | FALSIFIED — validator never passed |
| gi_raw still 0,0,0 → v6a branch | **CONFIRMED** |

The v10 cycle concluded source/binary mismatch is the most likely root cause (the binary's spdlog line-number reports match current source at lines 171/383 but DO NOT match the v3 diagnostic instrumentation at lines 460-564). v11 cannot resolve source/binary mismatch from the cron (terminal blocked, no build) — but v11 CAN apply a dormant code patch that, on the next parent rebuild, produces a guaranteed-visible diagnostic surface regardless of which of the three v9 explanations turns out to be true.

## The patch (already applied in working tree)

**File 1: Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp**

Added `#include <iostream>` (was missing; cerr would not have compiled).

Added 13 lines inside `FGIPass::DispatchRays()`, immediately after the function signature and before the v3 EARLY-RETURN guard:

```cpp
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

**File 2: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp**

Added `#include <iostream>` (was missing; cerr would not have compiled).

Added 10 lines inside `Render()`, immediately after the function signature and before the early-return guard:

```cpp
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

## diff_estimate

- `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp`: +14 / -0 (1 include + 13 #ifdef block)
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`: +11 / -0 (1 include + 10 #ifdef block)
- **Total: +25 / -0 lines, 0 lines of behavior change when HLVM_FORCE_CERR_LOGGING is undefined**

## skip_plan_review

no — patch is dormant but changes the binary size and adds 2 new headers + 2 new #ifdef blocks. Plan-criticer must sign off on (a) the include changes (adding `<iostream>` is a small cost but not zero), (b) the macro gating pattern, (c) the placement of the cerr writes (must be BEFORE any guard that could return early).

## test_strategy

No new test files needed. The patch is observable only when (a) the binary is rebuilt, AND (b) HLVM_FORCE_CERR_LOGGING is defined. Default behavior is identical to v10.

The validator (`validate_restir_gi.py`) continues to apply unchanged against post-rebuild dumps.

## risks

- **Lowest possible when HLVM_FORCE_CERR_LOGGING is undefined (default).** Patch is dormant; renderer state is identical to v10. Binary size grows by ~600 bytes (the included `<iostream>` template machinery). Negligible.
- **If macro is undefined and parent rebuilds**, the v3/v4a/v5/v6/v7/v8 patches should make the existing v3 diagnostic logs (Pre-GIPass, Post-GIPass, FGIPass::DispatchRays ENTER/EXIT/...) appear in spdlog. If they DO appear, the cerr patch is not needed and can be removed in a follow-up cycle.
- **If macro IS defined and parent rebuilds**, the cerr writes appear in stderr; if they appear BUT the spdlog logs do NOT, the bug is spdlog-level-filter and the next cycle wraps the v3 instrumentation differently.
- **If parent doesn't rebuild, the patch has no observable effect.** Pure file-only cycle from cron's perspective.
- **`<iostream>` include cost**: adds the iostream translation unit (heavy header). For Debug builds this is fine; for Release builds with `-Os`/`-O2` the unused template instantiations may be DCE'd. Acceptable.

## files

This cycle:
- `docs/PENDING_PLAN_v11.md` (this file)
- `docs/PENDING_PLAN_REVIEW_v11.md` (plan-critique)
- `docs/PENDING_COMMIT_v11.md` (impl summary: 25 lines added across 2 files, dormant by default)
- `docs/PENDING_IMPL_REVIEW_v11.md`
- `docs/PENDING_TESTS_v11.md`
- `docs/PENDING_TEST_AUDIT_v11.md`
- `docs/PIPELINE_HEALTH_2026-07-27.md` (append this tick's section)
- `docs/PENDING_PICK.md` (mark v11 [x], keep v6a-2/v6a-d/v11b/c/d decision matrix as the next-step options)

Source files modified (already patched this tick):
- `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` (+14 lines: 1 include + 13 #ifdef block)
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` (+11 lines: 1 include + 10 #ifdef block)

## What parent must do (priority-ordered)

1. **Rebuild WITHOUT the macro** (most likely path; default behavior):
   - `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
   - Run: `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal`
   - Expect: spdlog fires the v3 markers per frame (Pre-GIPass, Post-GIPass, FGIPass::DispatchRays ENTER, per-frame binding set OK, EXIT — 5/frame × 8 frames = 40 fresh log lines).
   - If markers now fire + gi_raw still 0 → v11b investigates specific dispatch body error.
   - If markers now fire + gi_raw non-zero + display correct → pipeline complete (v6d).
   - If markers STILL don't fire (impossible given line-number evidence, but worth checking) → v11c escalates to deeper cerr writes (bypass spdlog entirely).

2. **Rebuild WITH the macro** (only if step 1's markers still don't fire, very unlikely):
   - `CXXFLAGS=-DHLVM_FORCE_CERR_LOGGING ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
   - Run as in step 1. The cerr writes fire in stderr.
   - If `[RGI] Render() entry` and `[RGI] FGIPass::DispatchRays() entry` appear in stderr but spdlog markers don't appear in log file → bug is spdlog-level-filter.

3. **Report v11 evidence back to cron** with one of:
   - "Markers fire + gi_raw still 0" → v11b (binding-set failure or auto-barrier)
   - "Markers fire + gi_raw non-zero + display correct + validator 3/3" → pipeline complete
   - "Markers STILL don't fire" → v11c (deeper cerr, spdlog bypass)

## v6a decision matrix updated for v11 forward

| Parent's evidence after rebuild | Next cycle |
|---|---|
| v3 spdlog markers now fire per frame + gi_raw still 0 | v11b: examine FGIPass.cpp's `err` log paths (binding-set creation, missing handles, executeCommandList failure) — v6a-2 hypothesis becomes dominant |
| v3 spdlog markers now fire per frame + gi_raw non-zero + display correct | **pipeline complete (v6d)** |
| v3 spdlog markers now fire per frame + gi_raw non-zero + display bad | v11d: investigate accumulate/tonemap chain |
| v3 spdlog markers STILL don't fire after confirmed rebuild | v11c: deeper cerr writes + bypass spdlog entirely |
| v3 cerr writes appear (macro defined) but spdlog markers don't | v11e: spdlog config issue (level filter) |
| Parent cannot rebuild | pipeline stuck at v11; cron records structural limitation honestly |

## Honesty caveats

- All 6 roles are the same head (single-profile, single-prompt host). KEEP verdicts are self-checks.
- This plan changes source code with the macro OFF. The diagnostic surface (v3 spdlog + v5 NOTE + v11 cerr) is fully in place.
- The hypothesis (source/binary mismatch) remains the most likely root cause as of v11; the cerr-patch is a belt-and-suspenders option for parents who want a guaranteed-bypass path on the next rebuild.
- The patch is purely additive. It can be reverted in a single follow-up cycle with no behavioral change once the renderer is fixed.
