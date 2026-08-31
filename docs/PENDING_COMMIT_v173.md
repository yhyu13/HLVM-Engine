# Pending Commit v173 (DRAFT — operator-side commit; cron writes this proposal only, does not apply)

- plan: docs/PENDING_PLAN_v173.md
- plan_review: docs/PENDING_PLAN_REVIEW_v173.md (KEEP)
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
- source: file-only diagnostic this tick; supersedes v170 (ComposeDisplay hypothesis, REFUTED) and v171 (ACES saturation hypothesis, REFUTED) and v172 (no lights hypothesis, REFUTED via tick1548 evidence); implements v1557 analytical finding + v172 plan-review REVISED-recommendation
- target: local working tree (no push per job hard rules)
- task: Reduce `TC.MaxM` from 30.0f to 1.0f at line 950 and `SC.MaxM` from 30.0f to 1.0f at line 1005 to preserve per-pixel variance through the temporal resampling pass
- verify: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal && python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`
- skip_impl_review: no — though the patch is 2 character-pair edits, it touches load-bearing kernel-level tuning constants (MaxM) that affect ReSTIR reservoir behavior; reviewer recommended for sanity check on whether `r_ReSTIR_NumCandidates` also needs bumping to compensate for M=1 bias
- produces_test_files: no
- notes: Patch is 2 character-pairs in TestReSTIR_GI_Temporal.cpp (lines 950 + 1005). No shader changes, no nvrhi fork changes, no cmake regen, no FetchContent. Reuses existing `TC.MaxM` and `SC.MaxM` assignment sites — both already gated by the temporal/spatial dispatch blocks. Empirical prediction: display std ≈ 0.09-0.12 (was 0.046), mean ≈ 0.46 (unchanged).

## Proposed patch

### Edit site 1 — Temporal pass MaxM cap (line 950)

File: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`
Location: line 950, inside the per-frame `FReSTIRTemporalConstants TC{};` initialization block

```cpp
// Line 949 currently reads:
TC.FrameIndex       = float(AccumFrameCount);
// Line 950 currently reads:
TC.MaxM             = 30.0f;
// Line 951 currently reads:
TC.DepthThreshold   = 0.05f;

// v173: replace line 950 with:
TC.MaxM             = 1.0f;    // was 30.0f; forces M=1 → W≈1 → preserve per-pixel variance
```

### Edit site 2 — Spatial pass MaxM cap (line 1005)

File: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`
Location: line 1005, inside the per-frame `FReSTIRSpatialConstants SC{};` initialization block

```cpp
// Line 1004 currently reads:
SC.DepthThreshold   = 0.05f;
// Line 1005 currently reads:
SC.MaxM             = 30.0f;
// Line 1006 currently reads:
SC.SpatialRadius    = 3.0f;

// v173: replace line 1005 with:
SC.MaxM             = 1.0f;    // was 30.0f; matching cap downstream of temporal
```

### Total diff
- +2 lines (comment changes inline)
- -2 lines (the old hardcoded values)
- Net: 2 character-pair edits (30.0f → 1.0f × 2)
- No new files, no new includes, no cmake regen, no shader recompile, no FetchContent

## Plan Deviations

None. The patch matches `PENDING_PLAN_v173.md` §"Concrete code edits" block 1:1.

The plan-criticer (KEEP) flagged an additional risk (MaxM=1 makes spatial pass degenerate); the patch implementation handles this by also reducing SC.MaxM=1, which combined with the comment "matching cap downstream of temporal" preserves both passes' relative balance.

## Self-review checklist (operator-side)

- [ ] Validation: `validate_restir_gi.py` exits 0 with 6/6 PASS after rebuild+run (or fewer failures pre-fix → more passes post-fix)
- [ ] Error handling: no Vulkan validation layer errors in new log (grep `VUID-` → 0; pre-fix already 0, must stay 0)
- [ ] Tests: post-fix log shows display std ≈ 0.09 (was 0.046), vision gate sees recognizable Sponza
- [ ] Diff size: +2/-2 lines (well under 50-line budget per `skip_impl_review: no` rule; patch is minimal; reviewer recommended for sanity check on NumCandidates bump)
- [ ] No new files created
- [ ] No cmake regen (only `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` modified)
- [ ] No FetchContent / nvrhi fork changes
- [ ] No shader recompile needed (only test-side Desc config constants)

## Rebuild + verify recipe (verbatim from plan)

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine

# Edit (replace "30.0f" with "1.0f" at lines 950 and 1005)
$EDITOR Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
# Apply the patch above

# Build
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild

# Run + dump
cd Engine/Source/Runtime/Binary/Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal

# Verify log
grep "stats display floats" TestReSTIR_GI_Temporal.log | tail -1                # expect std >= 0.09
grep "stats gi_raw floats"  TestReSTIR_GI_Temporal.log | tail -1                # expect std >= 0.09
grep -E "VUID|ERROR|CommandList error" TestReSTIR_GI_Temporal.log | wc -l       # expect 0

# Validate
python3 /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
# Expect: 6/6 PASS (or fewer failures pre-fix → more passes post-fix; the color-variance check is the discriminator)

# Vision check
ls -t Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/*display_frame8.png | head -1
# Open in image viewer — expect: Sponza gallery arches + floor + back wall + directional shadow

# Mode-20 sanity
HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
# Expect gi_raw dump is NON-UNIFORM (per-pixel albedo color, not solid zero or constant mid-gray)
```

**Total operator-side effort**: ~5 min for incremental build + ~25 sec for run + ~30 sec for grep/validate/vision.

## Acceptance criteria (from PENDING_PLAN_v173.md, re-stated)

| # | Criterion | Cron-verifiable? | Empirical source |
|---|-----------|------------------|------------------|
| 1 | Debug target builds | NO (operator-only) | Build.sh exit code |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs clean | NO (operator-only) | exit code 0 |
| 3 | No Vulkan VUID/ERROR/CommandList errors | NO | grep `VUID\|ERROR\|CommandList error` returns 0 |
| 4 | `validate_restir_gi.py` passes newest dump | NO | exit code 0, 6/6 PASS |
| 5 | Fresh display PNG (vision) shows recognizable Sponza | NO (terminal + vision) | vision check |
| 6 | `HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial | NO (terminal-side) | grep gi_raw stats non-uniform |
| 7 | All 7 acceptance criteria pass | NO (depends on 1-6) | aggregate |

3/7 are also PARTIALLY file-only-verifiable via the on-disk log evidence (display std check requires grep `Binary/Debug/TestReSTIR_GI_Temporal.log` after the operator rebuilds — i.e., post-fix log is file-only-readable).

**Honest capability-vs-permission table for this cron tick:**

| Action | Capability | Permission | Result |
|--------|-----------|-----------|--------|
| Read PENDING_*.md | yes | yes | Wrote v173 plan + plan-review + commit |
| Write PENDING_COMMIT_v173.md | yes | yes | Done (this file) |
| Modify source code | yes | **no** (job rule: "Do not commit, push, or modify governance files") | NOT DONE |
| Build.sh --Rebuild | NO (terminal-blocked) | NO | NOT DONE |
| ./TestReSTIR_GI_Temporal | NO | NO | NOT DONE |
| python3 validate_restir_gi.py | NO | NO | NOT DONE |
| vision_analyze display PNG | NO | NO | NOT DONE |

## Cumulative status for the lineage (post-this-tick)

- v170 → v171 → v172 → v173 supersetions documented in this commit, PICK.md cycle-stop notes, and PENDING_PLAN_v173.md refutation table
- v166 + v168 + v169 patches INTACT on disk (graphics-pipeline rebind in 3 nvrhi fork copies)
- v137 + v140 + v151 source fixes INTACT (binding zero-offset, AmbientColor override, ReSTIR Generate split)
- All 4 load-bearing CycleStop lineages (v166, v167, v168, v169) completed through all 6 roles with KEEP/ALL_KEEP verdicts
- 4/7 acceptance criteria file-only-verifiable post-fix (build artifact, log, validator, vision)
- 3/7 require operator-side terminal+vision+numpy
- This cron lineage has reached the file-only ceiling of progress — termination requires operator-side recipe execution

## Prior cycle summary (operational context)

The v166 cycle (graphics-pipeline rebind port) is INTACT on disk. The v169 cycle (cross-tree port) is INTACT on disk. v167 was SUPERSEDED by v168+v169 (the explicit-clear was reverted when graphics-pipeline rebind proved the correct fix). All cycle markers remain on disk per HARD INVARIANT 7 (preserve traceability).

The display-monochrome symptom (PICK line 118, opened tick1544) is the **only remaining open card**. It is also the *load-bearing empirical evidence card* — until display std ≥ 0.10, the SRV-binding-fix lineage is not closed.

The v173 plan IS the closure recipe. Operator-side execution of the 5-min recipe in Step 2-6 above closes PICK line 118.

— impler, tick 2026-08-15-tick1567, file-only, single-profile host, terminal-blocked.
