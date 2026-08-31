# Pending Plan v13 — add UAV-write sentinel debug mode 6 to GIPathTracing.hlsl as the next mechanically actionable file-only fix (decisive dispatch-body-vs-UAV-write test)

- task: add a new debug mode (case 6u) to the GIPathTracing.hlsl raygen's `switch (debugMode)` ladder. The new case writes a UNIQUE, recognizable per-pixel constant to OutputTexture (and ONLY to OutputTexture) — bypassing all SRV reads, TraceRay, lighting math, and ClosestHit. If `gi_raw` with `HLVM_PT_DEBUG_MODE=6` shows the per-pixel constant, the dispatch body is running and the UAV write is reaching OutputTexture. If it shows 0, the dispatch is not running (H-A: source/binary mismatch means binary lacks the v3 instrumentation; or the v12 cerr patch will tell us). If it shows garbage (not the per-pixel constant, not 0), the UAV write is being partially-overwritten by something downstream (e.g., a follow-up compute pass that doesn't know about the debug mode).
- source: docs/PENDING_PLAN_v12.md (v12 cerr patch in place) + the on-disk log evidence in `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log` (2026-07-27 00:07 run) + docs/PENDING_PICK.md v12a/c/e decision matrix
- approach: single HLSL file edit. 1 new switch case (case 6u) in the existing debug-mode ladder at line 578-597 of GIPathTracing.hlsl. No other code touched. No C++ side change required. The debug mode is selected via `HLVM_PT_DEBUG_MODE=6` env var (already wired through the C++ at FGIPass.cpp:446-449).

## Why this is the right v13 cycle

The v12 patch (cerr default-ON) is in source. The binary on disk is stale. The next parent rebuild will:
1. Fire 16 cerr lines (8 Render + 8 FGIPass::DispatchRays) regardless of spdlog behavior.
2. If v3 spdlog markers NOW fire (H-A confirmed) — the binary was simply stale.
3. If v3 spdlog markers STILL don't fire (H-B confirmed) — spdlog config issue.
4. If both fire but gi_raw still 0 — the GI dispatch body is reached but its UAV write is being dropped (H-C: a separate bug in the dispatch body or downstream).

v13 (this patch) targets case (4). With `HLVM_PT_DEBUG_MODE=6`:
- The raygen body runs the dispatch.
- It writes a known per-pixel constant directly to OutputTexture.
- Nothing else (no SRV reads, no TraceRay, no lighting math) is executed.

If case (4) is true and the v13 patch is in source at the time of the next rebuild, the next parent run with `HLVM_PT_DEBUG_MODE=6` will surface the issue. If mode 6 shows the per-pixel constant: dispatch body is fine, the bug is in lighting/payload math. If mode 6 shows 0: dispatch body is not being reached (H-A or H-B dominates). If mode 6 shows garbage: UAV write is being overwritten by something downstream.

## The patch

**File: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl`**

Add a new `case 6u:` to the existing debug-mode switch (verified via read_file at offset 575-599):

```hlsl
case 6u:  debugColor = float3(float(pixel.x) / 256.0, 0.0, float(pixel.y) / 256.0); break;
```

The existing comment block (lines 584-592 of the post-patch file) documents the purpose.

Net: +10 lines (the comment + the case statement). The existing case-6u is unused; assigning it doesn't conflict with anything.

### Why mode 6u is the right value

The existing debug modes 1u-5u are taken (albedo, normal, primary-direct, indirect, hitDist). Mode 6u is unused. Modes 7u-12u are also unused but skipped because the next-in-line free mode is 6u.

The constant `(float(pixel.x) / 256.0, 0.0, float(pixel.y) / 256.0)` is recognizable per-pixel: at the 800x600 Sponza frame, R varies in [0, 800/256≈3.125] and B varies in [0, 600/256≈2.34]. The dump will show a recognizable gradient. The G channel is always 0, so any "not 0" result means the dispatch wrote something.

### Why this test is decisive

| `HLVM_PT_DEBUG_MODE=6` result | What it means |
|-------------------------------|---------------|
| Per-pixel gradient visible (R=0..3, G=0, B=0..2) | Dispatch body runs, UAV write lands. Bug is in lighting/payload math. |
| All zeros (R=G=B=0) | Dispatch body not reached, or UAV write dropped. Source/binary mismatch (H-A) likely. |
| Garbage (e.g. NaN, sentinel value, fully random) | UAV write is being overwritten or the debugColor assignment is being optimized away by slangc. |
| Single uniform value (e.g. all pixels = (1, 1, 1)) | Something is reading OutputTexture post-write and writing a uniform value. |

The pre-condition for this test to be informative: the binary on disk must include the v13 patch (so the next parent rebuild is required). v13 is in source starting this tick.

## diff_estimate

- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl`: +10 / -0 (1 case statement + 9-line comment)
- **Total: +10 / -0 lines, 0 lines of behavior change when HLVM_PT_DEBUG_MODE is unset**

## skip_plan_review

no — patch changes shader source semantics. Plan-criticer must sign off on:
- (a) the new case 6u doesn't conflict with existing cases (it doesn't, slot is free)
- (b) the per-pixel constant is recognizable in the dump (R=0..3, G=0, B=0..2)
- (c) the early-return at line 466-469 (length(worldPos) < 0.001) still bypasses mode 6 for background pixels — acceptable because most Sponza pixels have valid worldPos
- (d) the NaN-safety fallback at line 568-571 (sets result to red) is overridden by mode 6 — correct because the goal of mode 6 is to confirm the dispatch body reaches the Output[pixel] assignment at line 600/601

## test_strategy

No new test files needed. The patch is observable only when (a) the binary is rebuilt, AND (b) `HLVM_PT_DEBUG_MODE=6` is set in the env.

### Parent-driven tests (terminal blocked in cron):

1. **Build cleanliness** (default rebuild): `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal`. Expected: clean build, no shader compile errors.

2. **Run with default env vars** (the v12 evidence path):
   - `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log`
   - Expected stderr.log: 16 cerr lines (8 Render + 8 FGIPass::DispatchRays) — confirms v12 patch is live
   - Expected TestReSTIR_GI_Temporal.log: v3 spdlog markers per frame IF H-A is true (binary was stale)

3. **Run with HLVM_PT_DEBUG_MODE=6** (the v13 evidence path):
   - `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=6 ./TestReSTIR_GI_Temporal 2>stderr.log`
   - Expected: gi_raw shows per-pixel gradient (R varies, G=0, B varies)
   - If gi_raw shows per-pixel gradient → dispatch body is fine, bug is downstream of the dispatch's Output[pixel] write
   - If gi_raw shows 0 → dispatch body not reached (H-A or H-B dominates; v12 cerr evidence will tell us which)

4. **Vision-analyze `display_frame8.png`** for recognizable non-uniform Sponza geometry (the same as v12's parent test).

5. **Run validator**: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`. Expected: 3/3 status.

## risks

- **Shader compile error if the existing GIPathTracing shader has changed since this patch was authored.** The patch targets the line numbers verified at read_file offset 575-599. If the shader has shifted, the patch may not apply. Mitigation: re-verify line numbers before applying.
- **slangc may dead-strip the case 6u branch.** Unlikely — the case is reached by `if (debugMode != 0u)` and `switch (debugMode)`. The case label itself is a jump target. But if slangc's optimization is aggressive enough, it could fold the case to a constant if `debugMode` is statically known. Mitigation: the C++ side writes `g_GI.Params5[0] = static_cast<float>(DebugMode)` (FGIPass.cpp:450), so `debugMode` is data-dependent, not constant.
- **Mode 6 may not bypass the early-return at line 466-469.** The early-return is BEFORE the debug-mode switch, so background pixels (length(worldPos) < 0.001) still get (0,0,0,1.0). This is acceptable for diagnosing the dispatch body — if most Sponza pixels have non-zero worldPos (verified by gbuffer_worldpos dump showing real geometry), then most pixels will execute the debug-mode switch.
- **Per-pixel constant may not survive DumpRGBA32FTexture's per-channel normalization.** The dump at line 1712 normalizes gi_raw per-channel. R=0..3 will normalize to R=0..1. The gradient will still be visible. This is the desired behavior.
- **If the binary is NOT rebuilt**, the patch has no observable effect. v12's cerr writes and v13's case 6u are dormant. Pure file-only cycle from cron's perspective.

## files

This cycle:
- `docs/PENDING_PLAN_v13.md` (this file)
- `docs/PENDING_PLAN_REVIEW_v13.md` (plan-critique)
- `docs/PENDING_COMMIT_v13.md` (impl summary: 10 lines added to GIPathTracing.hlsl)
- `docs/PENDING_IMPL_REVIEW_v13.md`
- `docs/PENDING_TESTS_v13.md`
- `docs/PENDING_TEST_AUDIT_v13.md`
- `docs/PIPELINE_HEALTH_2026-07-27.md` (append this tick's section)
- `docs/PENDING_PICK.md` (mark v13 [x], keep v12a/c/e/v13a decision matrix as next-step options)

Source files modified:
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` (+10 lines: 1 case statement + 9-line comment)

## What parent must do (priority-ordered)

1. **Rebuild WITHOUT HLVM_PT_DEBUG_MODE** (v12 evidence path): confirm v12 cerr writes fire and v3 spdlog markers either fire (H-A confirmed) or don't (H-B confirmed).
2. **Rebuild with HLVM_PT_DEBUG_MODE=6** (v13 evidence path): confirm whether gi_raw shows the per-pixel gradient or 0/garbage.
3. **Report combined evidence back to cron** with one of:
   - "cerr fires + v3 spdlog markers NOW fire + gi_raw still 0 with mode=0" → H-A confirmed; next step is the v13 mode=6 result
   - "cerr fires + v3 spdlog markers STILL don't fire + gi_raw still 0" → H-B confirmed; spdlog config fix
   - "mode=6 shows per-pixel gradient + mode=0 gi_raw=0" → dispatch body fine, bug is in lighting/payload math; v13a investigates the lighting math
   - "mode=6 shows 0" → dispatch body not reached; v12 evidence tells us why
   - "mode=6 shows garbage" → UAV write is being overwritten; v13b investigates
4. **Vision-analyze display_frame8.png + run validator**.

## v13 decision matrix (post-rebuild evidence)

| Parent's v12+v13 evidence | Next cycle |
|---------------------------|------------|
| cerr fires + v3 spdlog markers NOW fire + mode=6 per-pixel gradient + mode=7 scene-shape × 1.5 + mode=0 gi_raw=0 | **v18**: investigate TraceRay / payload / SRV-read chain (case 8u TraceRay-only sentinel) |
| cerr fires + v3 spdlog markers NOW fire + mode=6 per-pixel gradient + mode=7 still 0 | **v18**: investigate GBufferMaterial SRV / uniform binds (mode-9 diffuse-only sentinel) |
| cerr fires + v3 spdlog markers NOW fire + mode=6 per-pixel gradient + mode=0 gi_raw non-zero + display correct | **pipeline complete (v6d)** |
| cerr fires + v3 spdlog markers NOW fire + mode=6 still 0 | dispatch body not reached; check why v3 ENTER log doesn't match mode=6 behavior. Most likely: dispatch body returns early or binding set fails. Investigate. |
| cerr fires + v3 spdlog markers NOW fire + mode=6 shows garbage | UAV write overwritten; investigate downstream compute passes (bilateral denoise, ReSTIR) for OutputTexture writes |
| cerr fires + v3 spdlog markers STILL don't fire | H-B confirmed; spdlog config fix (v12e) |
| cerr does NOT fire | v12c: stderr not reaching stream |
| Parent cannot rebuild | structural block persists; cron records honestly |

## Honesty caveats

- All 6 roles are the same head (single-profile, single-prompt host). KEEP verdicts are self-checks.
- This patch changes shader source semantics (new debug case). The default mode=0 behavior is unchanged.
- The hypothesis (gi_raw=0 is a dispatch-body vs UAV-write vs lighting-math problem) is informed by the 00:07 log evidence. The v13 patch is a decisive test for the dispatch-body-vs-UAV-write half of the question. The lighting-math half requires a different probe (e.g., forcing the shader to skip the TraceRay and return a known value via the diffuse * AmbientColor * AmbientScale path).
- The patch is purely additive. It can be reverted in a single follow-up cycle with no behavioral change.
- The cron's terminal is still blocked (tirith denies every terminal command). v12 and v13 patches are in source but their effects require a parent rebuild to observe.
