# Pending Tests v176

- plan: docs/PENDING_PLAN_v176.md
- commit: docs/PENDING_COMMIT_v176.md
- impl_review: docs/PENDING_IMPL_REVIEW_v176.md (KEEP)
- test_scope: operator-side verification (the test file IS `TestReSTIR_GI_Temporal.cpp`; no new test files produced by v176)
- test_strategy: 5-min operator-side recipe (re-uses and extends the recipe in `PENDING_COMMIT_v176.md` §"Rebuild + verify recipe") + automated 4-check structural validator (`validate_restir_gi.py`) + vision check + mode-20 discrimination run
- timestamp: 2026-08-17T-tick-now-86-Z

## Summary

The v176 commit does NOT produce new test files (`produces_test_files: no` in `PENDING_COMMIT_v176.md`). The test file for the v176 change is the existing `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` itself (which IS the test driver). The tester's deliverable for v176 is therefore:

1. **List of test files** (zero new files; the existing `TestReSTIR_GI_Temporal.cpp` is the test).
2. **Test scenarios** to verify the v176 patch (operator-side 5-min recipe).
3. **Pre-existing test infrastructure** that the v176 patch runs through.

## Test files (no new files produced)

| File | Role | Modified by v176? |
|------|------|--------------------|
| `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` | Test driver (the Sponza + ReSTIR pipeline) | **Yes** (4 edits, +3 net lines) |
| `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` | 4-check structural validator (black%, color variance, temporal stability, cell variance) | No |
| `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py` | Per-pixel statistics analysis on dumps | No |
| `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/test_validate_restir_gi.py` | Self-test for the validator | No |

Per HARD INVARIANT #2 ("Test files always trigger the reviewer"): v176's `produces_test_files: no` is correctly set. The test file for v176 is the test driver itself. The 5-min operator recipe is the verification surface.

## Test scenarios (operator-side, 5 minutes)

### Scenario 1: Build verification (60 sec)

**Setup**: apply the v176 patch (4 edits) to `TestReSTIR_GI_Temporal.cpp`.

**Command**:
```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild --Jobs=4
```

**Expected**: clean build, exit 0, no compiler errors. Any `error:` line in stderr fails this scenario.

**Pass criterion**: exit 0 and the binary `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal` exists and is executable.

### Scenario 2: Env-var hook fires (5 sec)

**Setup**: env var `HLVM_RGI_MAXM=1.0` is set.

**Command**:
```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Runtime/Binary/Debug
HLVM_RGI_MAXM=1.0 ./TestReSTIR_GI_Temporal 2>&1 | grep "HLVM_RGI_MAXM override"
```

**Expected log line** (from the v176 env-var hook):
```
[info] HLVM_RGI_MAXM override: r_ReSTIR_MaxM = 1.00
```

**Pass criterion**: the grep finds the log line. If grep returns 0 matches, the env-var hook did not fire (multi-instance CVar failure or SetValue rejected).

### Scenario 3: Display std rises from 0.046 to ≈ 0.09 (25 sec)

**Setup**: dump the frame buffer with `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8`.

**Command**:
```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Runtime/Binary/Debug
HLVM_RGI_MAXM=1.0 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
grep "stats display floats" TestReSTIR_GI_Temporal.log | tail -1
```

**Expected log line** (from the test's per-frame stats line):
```
[info] stats display floats — mean=0.XX std=0.YY min=... max=...
```

**Pass criterion**: `std ≥ 0.09` (per the v173 pre-edit log analysis: pre-temporal std=0.091-0.120, post-temporal std=0.0457 — the v176 hypothesis is that `MaxM=1.0f` preserves per-pixel variance through the temporal resampling pass, raising post-temporal std from 0.0457 back to ≈ 0.09).

**Pre-edit (v173 patch as-shipped) baseline**: std ≈ 0.0457 (post-temporal). v176 hypothesis: std should rise to ≈ 0.09 after the env-var→CVar round-trip.

### Scenario 4: No Vulkan VUID/ERROR (5 sec)

**Command**:
```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Runtime/Binary/Debug
grep -E "VUID|ERROR|CommandList error" TestReSTIR_GI_Temporal.log | wc -l
```

**Expected**: 0 (zero matches).

**Pass criterion**: `wc -l` returns 0. Any VUID or ERROR is a fail (the v176 patch is test-side only and should not introduce new validation errors).

### Scenario 5: 4-check structural validator passes (15 sec)

**Command**:
```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
```

**Expected**: 6/6 PASS (or fewer failures pre-fix → more passes post-fix). The 4 structural checks:
1. **Black-pixel ratio < 5%** — shadows allowed, full-black not
2. **Color variance > some floor** — per-channel spatial std
3. **Temporal stability < some ceiling** — max step between consecutive frame means
4. **Cell variance > some floor** — split image into NxN grid; std of cell-means

**Pass criterion**: at least 4/4 structural checks PASS. The validator may report more (e.g., 6/6) if pre-existing edge cases also pass. The pre-fix state had 0/4 to 2/4 PASS (uniform-color gi_raw failed color variance and cell variance).

### Scenario 6: Vision check — recognizable Sponza (30 sec)

**Command**:
```bash
ls -t /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/*display_frame8.png | head -1
# Open the file in an image viewer (or use vision_analyze on it)
```

**Expected**: the display PNG shows recognizable Sponza geometry — back wall, upper gallery arches, lower floor arches, with sane exposure (not blown-out white, not crushed black, not uniform color).

**Pass criterion**: a human viewer (or vision_analyze) confirms the image is recognizable Sponza with sane exposure. Per the skill's "4-check structural validator > scalar mean-luma gate" rule: the scalar mean-luminance gate is not sufficient; vision confirmation is required.

### Scenario 7: Mode-20 GBufferMaterial non-zero (25 sec)

**Setup**: rerun with `HLVM_PT_DEBUG_MODE=20` to discriminate the GBuffer SRV binding (the original bug from DIAGNOSTIC_2026-07-30.md).

**Command**:
```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Runtime/Binary/Debug
HLVM_PT_DEBUG_MODE=20 HLVM_RGI_MAXM=1.0 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
# Per-channel stats on the gi_raw dump:
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py \
  --data-dir Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data
```

**Expected**: gi_raw per-channel stats show non-zero values (NOT uniform `(0, 0, 0)` and NOT uniform `(1, 1, 1)`). Pre-v139 (before the SRV binding fix), mode 20/21/22 returned uniform zero. Post-v139, they return real GBuffer data.

**Pass criterion**: dump_pixelstats shows non-zero per-channel std and a wide range of unique values. If the dump shows uniform values, the GBuffer SRV binding has regressed.

## Pre-existing test infrastructure (re-used by v176)

| File | Used by v176 scenario(s) |
|------|---------------------------|
| `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` | Scenario 5 (4-check structural validator) |
| `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py` | Scenario 7 (mode-20 pixel stats) |
| `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/test_validate_restir_gi.py` | Self-test for the validator (not a v176 scenario) |
| `Engine/Source/Common/Test/Test.h` | The test framework (provides `RECORD_BOOL(test_ReSTIR_GI_Temporal)` macro) |
| `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` | The test driver itself (v176's 4 edits modify this file) |

No new test infrastructure is needed for v176. The patch is +3 net lines; the test surface is the existing test driver + 2 Python validator scripts.

## Pass/fail criteria summary

| # | Scenario | Pass criterion | Fails on |
|---|----------|-----------------|----------|
| 1 | Build | exit 0, binary exists | compiler error, linker error |
| 2 | Env-var hook fires | grep finds `HLVM_RGI_MAXM override: r_ReSTIR_MaxM = 1.00` | multi-instance CVar failure, SetValue rejected (ReadOnly flag — not the case here) |
| 3 | Display std ≥ 0.09 | grep finds `std=0.09X` (X ∈ {0..9}) | env-var hook didn't propagate to per-frame block; v176 hypothesis wrong |
| 4 | No VUID/ERROR | `wc -l` returns 0 | Vulkan validation mismatch (should not happen — v176 is test-side only) |
| 5 | Validator 4/4 PASS | validator exits 0 with 4/4 (or 6/6) PASS | color variance < floor, cell variance < floor (uniform-color image) |
| 6 | Vision: recognizable Sponza | human/vision sees Sponza with sane exposure | uniform gray/white, blown highlights, crushed blacks |
| 7 | Mode-20 non-zero GBufferMaterial | dump_pixelstats shows non-zero per-channel std and unique values | uniform values (SRV binding regressed) |

## Closure decision

- **All 7 scenarios PASS** → v176 is closed. ReSTIR GI repair lineage closed (v2 → v137 → v140 → v142 → v151 → v166 → v168 → v169 → v173 → v176). All 7 user acceptance criteria satisfied.
- **Scenarios 1-2 PASS, 3 FAIL** (display std < 0.07) → v176 hypothesis wrong. Fall back to v174 (AmbientScale=0.10 + NumCandidates=16).
- **Scenarios 1-2 PASS, 3 PASS, 5 FAIL** (validator) → v176 makes display std right but color variance still wrong. Likely a tone-mapping or denoiser issue, NOT a v176 issue. Investigate denoiser pass.
- **Scenarios 1-2 PASS, 3 PASS, 5 PASS, 6 FAIL** (vision) → scalar metrics say PASS but human sees monochrome. Per the skill's "4-check structural validator" rule, the human vision check is the final gate. Likely an exposure clamp or tonemap issue, NOT a v176 issue.
- **Scenario 1 FAIL** (build) → the v176 patch has a syntactic error or includes a wrong file path. The impler's manifest may have a typo. FIX the impl manifest, re-derive the patch.
- **Scenario 2 FAIL** (env-var hook didn't fire) → multi-instance CVar broke the env-var path. The test's local CVar instance is separate from the one read in the per-frame block. This would be a structural issue with the v176 patch; v174 fallback.

## State machine routing

**This tick's role**: tester (consistent with state machine Rule 7, impl_rev KEEP, no tests → tester).

**Verdict**: tests staged. 7 operator-side scenarios with clear pass/fail criteria. No new test files produced (the v176 patch is in the test driver itself).

**Next tick's routing**: Rule 8 (tests exist, audit None) → **testing-verifier**. The testing-verifier produces `docs/PENDING_TEST_AUDIT_v176.md` with the verdict (ALL_KEEP / SOME_RELAX / SOME_DELETE / MAJOR_DELETE) on the test scenarios. The audit is a meta-review of the test scenarios themselves: are they complete? Are the pass criteria clear? Are there missing edge cases? Do they actually exercise the v176 patch? This is the gate that catches "the test passes but doesn't actually test the fix" failures.

## Carry-forward

- v176 plan: KEEP'd (tick-83). v176 commit: KEEP'd (tick-85). v176 tests: this tick.
- v176 test audit: next marker.
- v176 closure: operator-side 5-min recipe, gated on Scenarios 1-7 all PASS.
- v173 patch INTACT on disk (will be replaced when the operator applies v176).
- v174 frozen fallback dormant (gated on Phase A FAIL).
- v175 (original, FIX'd) and v175 v2 (folded into v176) — both cycles closed.
- Operator-side execution still blocked by tirith (`terminal` denied, cumulative 1871+ denials per this lineage).
- dumps directory empty (no fresh test run since v173 patch landed on 2026-08-15).
- The 5-minute operator recipe in `docs/PENDING_TESTS_v176.md` is the closure gate.

— tester, 2026-08-17, tick-now-86, single-profile host, terminal-blocked, autonomous invocation #26.
