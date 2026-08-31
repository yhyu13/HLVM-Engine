# Pending Plan v142
- task: v142 — Revert v141's `Desc.AmbientScale = 0.25f` back to `1.5f` in `TestReSTIR_GI_Temporal.cpp` and replace the v141 REFINED DIAGNOSIS comment with a v25-diagnostic-aligned comment, while keeping v140's `Desc.AmbientColor = (1, 1, 1, 0)` override intact. This is a single-line surgical revert based on log evidence showing v141 over-darkened the image (95× dynamic range pre-v141 collapsed to 2× post-v141).
- source: `docs/PIPELINE_HEALTH_2026-08-01_tick534.md` (this is the authoritative v25-diagnostic-correction finding, the most recent on-disk log re-read showing the v141 regression), `docs/PIPELINE_HEALTH_2026-08-02.md` (v25-diagnostic-stale finding), and the three log files at `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal{,_1,_2}.log` line 320-321 showing the dynamic range progression across 3 binary runs.
- approach:
  1. **Revert `Desc.AmbientScale` from `0.25f` to `1.5f`** at `TestReSTIR_GI_Temporal.cpp:464`. This is the single functional-line change. The v140 value of `1.5f` is restored.
  2. **Replace the v141 REFINED DIAGNOSIS comment** (16 lines, lines 447-463) with a v25-diagnostic-aligned comment that documents the post-revert reasoning: the 4-light NEE infrastructure (1 Directional + 3 Point at `FGIPass.cpp:380-395`) was already producing per-pixel variation; v141's reduction was based on a faulty v25 inference. The new comment should be ~4 lines, not 16.
  3. **Keep v140's `Desc.AmbientColor = (1, 1, 1, 0)` override** at lines 470-473 unchanged. v140 was the AmbientColor override (a benign refinement matching the test author's documented intent at the original `TestReSTIR_GI_Temporal.cpp:431-441` comment). v140 stays.
  4. **Net patch**: -16 / +4 lines in `TestReSTIR_GI_Temporal.cpp` only. No CMakeLists.txt change. No other source files touched. All 11 prior patches (v22 split + v131-v139 + v140) remain intact.
- diff_estimate: -16 / +4 lines (1 file: `TestReSTIR_GI_Temporal.cpp`)
- skip_plan_review: no (default; though surgical, the reasoning chain warrants a critic check)
- skip_impl_review: yes (per `HARD INVARIANT #2`: only honored because `produces_test_files: no` — this is a single-line revert in production test code, no new test files; surgical patch, not architectural)
- produces_test_files: no
- test_strategy: file-only patch integrity verification via `read_file` + `search_files` (the tester role checks the new AmbientScale value, the comment block text, the v140 AmbientColor override remains, and the absence of incidental mutation to v131-v140 sites — see `PENDING_TESTS_v142.md`)
- risks:
  1. **Test determinism issue not addressed by v142**: tick 534 identified that the post-v141 binary produced inconsistent results between two runs 90 seconds apart (23:15: 95× dynamic range vs 23:17: 2× dynamic range). v142 reverts AmbientScale but does not address the determinism issue. If the determinism issue is a state-persistence bug (e.g., uninitialized accum buffer state, temporal ReSTIR pass not resetting), v142 alone won't close the bisect. **Mitigation**: parent-runspace vision check on the post-v142 binary's display_frame8.png is the discriminator. If it shows recognizable Sponza with sane exposure, bisect closes. If it shows garbage or uniform color, v143 must investigate the determinism issue separately.
  2. **Over-bright image post-revert**: with `Desc.AmbientScale = 1.5f` + `Desc.AmbientColor = (1, 1, 1)`, the per-pixel `primaryAmbient = diffuse * (1, 1, 1) * 1.5` ranges roughly `(0.7*1.5, 0.7*1.5, 0.7*1.5) = (1.05, 1.05, 1.05)` to `(1.0*1.5, 1.0*1.5, 1.0*1.5) = (1.5, 1.5, 1.5)` per pixel (diffuse is GBufferMaterial which varies (0.7, 0.7, 0.7) to (1.0, 1.0, 1.0) per `TestReSTIR_GI_Temporal.cpp:766-768`). Plus `primaryDirect` from 4 lights (varying 0..2 per pixel). The total per-pixel `result` can hit `(3.5, 3.5, 3.5)` in lit areas. The dump clamps to [0, 1] so the displayed PNG will saturate. **Mitigation**: the v25 diagnostic explicitly noted "Sponza GLTF loads white materials" — the pre-v141 image at 19:46 (95× dynamic range) DID show the structure via per-pixel variation in the [0, 1] clamp range, with lit areas saturating to white. This is acceptable for the acceptance criteria ("sane exposure" allows for bright lit areas as long as the per-pixel variation is visible in non-saturated regions). If the post-v142 image is uniformly white, the AmbientColor override (v140) may need to be reverted too — that's a v143 question.
  3. **v141 comment block too short to capture the v25-correction reasoning**: a 4-line v25-aligned comment may not document the 4-light NEE infrastructure adequately. **Mitigation**: the 4-line comment should reference `FGIPass::UploadLights()` at line 380+390-395 explicitly so future maintainers can verify the light infrastructure.
  4. **Single-profile self-check**: all 6 roles on this host run as the same head; the plan-criticer / testing-verifier verdicts are self-checks per `Anti-pattern #7`. The patch is small enough (1 functional line + 4-line comment) that this is acceptable.

## Files modified (expected)

| File | Change | Approx lines |
|------|--------|--------------|
| `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` | -16 / +4 lines: replace `Desc.AmbientScale = 0.25f` (line 464) + v141 REFINED DIAGNOSIS comment (lines 447-463) with `Desc.AmbientScale = 1.5f` + v25-aligned comment | -16/+4 |

Net: -12 lines, single file. Diff: ~12 lines, surgical.

## Verification (parent runspace only)

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine

# Step 1: Rebuild
./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug
# Expected: SUCCESS (only TestReSTIR_GI_Temporal.cpp changed, no new source files)

# Step 2: Run with the spec env vars
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal
# Expected: gi_raw log line "R[<min>,<max>] G[<min>,<max>] B[<min>,<max>]"
# with min<max (v141 produced R[0.0, 2.0]; v142 expected R[<low>, <high>] with high>10)

# Step 3: Per-pixel statistics
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py \
    --data-dir Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data
# Expected: gi_raw per-channel mean > 5.0 (per validate_restir_gi.py check 1)

# Step 4: 4-check structural validator
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
# Expected: 4/4 checks PASS (non_black_channel_mean, spatial_std, cell_variance, alpha_sentinel)

# Step 5: Mode 20 discriminator
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 ./Binary/Debug/TestReSTIR_GI_Temporal
# Expected: per-pixel GBufferMaterial read returns non-zero data

# Step 6: Vision check on the fresh display PNG
# Open Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/<newest>_display_frame8.png
# Expected: recognizable Sponza with sane exposure
```

## Acceptance for v142 itself (file-only verifiable)

1. `TestReSTIR_GI_Temporal.cpp:464` contains `Desc.AmbientScale      = 1.5f;` (1 match, replacing the v141 `0.25f`)
2. `TestReSTIR_GI_Temporal.cpp:447-451` contains the v25-aligned comment (4-6 lines), no longer the 16-line v141 REFINED DIAGNOSIS
3. `TestReSTIR_GI_Temporal.cpp:470-473` retains v140 `Desc.AmbientColor[0..3] = (1.0f, 1.0f, 1.0f, 0.0f)` override (4 matches, unchanged)
4. No other call sites broken
5. FGIPass.h:58-62 v140 AmbientColor field intact (default `{ 0.6f, 0.6f, 0.65f, 0.0f }`)
6. FGIPass.cpp:447-449 + 463 v140 AmbientColorPtr indirection intact
7. All 11 prior patches (v22 split + v131-v140) still intact
8. CMakeLists.txt UNCHANGED (no new source files; TestReSTIR_GI_Temporal.cpp already in target)

## Routing implications

This is a file-only revert (no new build required at the planner level). The plan is necessary to advance the bisect because the v141 patch is now confirmed to be a regression (collapsed dynamic range from 95× to 2×). v142 reverts the regression while preserving the binding fix (v131-v139) and the benign v140 AmbientColor override.

**Critical caveat**: v142 does NOT address the test determinism issue (23:15 vs 23:17 inconsistency in the post-v141 binary). If post-v142 the image is recognizable Sponza, bisect closes. If post-v142 the image is uniform or unrecognizable, v143 must investigate the determinism issue separately (likely in the GPU temporal ReSTIR pass or the accum buffer state initialization).

## Honest interpretation of the user's autonomous instruction

The user said: "Continue iterating until all criteria met or report concrete external blocker with evidence." v142 is the iteration step that follows from the v25-diagnostic-correction finding. The 5 of 7 user acceptance criteria that are NOT file-only verifiable (vision check, validator pass, mode 20 non-zero return, dynamic range > 5×, fresh display recognizable Sponza) all require terminal+vision+numpy. v142 is the file-only patch step; the parent runspace executes the v142 verify recipe to either close the bisect or surface v143's discriminator.
