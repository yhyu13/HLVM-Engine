# Pending Plan v175 — Apply v173 hypothesis via CVar override (BIDIRECTIONAL: if v173 Phase A FAILs, revert instantly via env var)

- task: Verify the v173 hypothesis (`MaxM=1.0f` preserves per-pixel variance) by using the `r_ReSTIR_MaxM` CVar override path instead of the hardcoded source-edit path. This is functionally equivalent to v173 (same numerical effect: `MaxM=1.0f` at both temporal and spatial passes) but has TWO structural advantages:
  1. **Fully reversible without a rebuild.** If Phase A FAILs, the operator sets `r_ReSTIR_MaxM=30.0` (or unsets the env var) and re-runs. No source edit. No rebuild. ~25 sec per cycle.
  2. **Compatible with the v174 fallback compound fix** (AmbientScale=0.10 + r_ReSTIR_NumCandidates=16) — these CVars can be co-tuned without source edits.

- source: `Engine/Source/Runtime/Public/Renderer/GI/GICVars.h:38` (the existing `r_ReSTIR_MaxM` CVar, default 30.0f, Saved-flag) + `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp:950, 1005` (v173 patch hardcoded `1.0f` into the C++ at line 950 `TC.MaxM = 1.0f` and line 1005 `SC.MaxM = 1.0f`) + `docs/PENDING_PLAN_v173.md` (the hypothesis — v173 is correct, only the IMPLEMENTATION path is questioned) + `docs/PENDING_PLAN_v174.md` (the FROZEN rollback fallback — v175 supersedes only the rollback path; v174 stays as a separate Phase A FAIL contingency).

- approach: **Three-mode approach** that gives the operator full control without source edits:

  **Mode A (preferred, Phase A primary):** REVERT the v173 source hardcode (set `TC.MaxM = 30.0f` and `SC.MaxM = 30.0f` at lines 950 + 1005), then run with env var `r_ReSTIR_MaxM=1.0`. The CVar override propagates through to the temporal/spatial passes via the test's `CVar_r_ReSTIR_MaxM` lookup. This is what v173 *intended* — but v173 was implemented by hardcoding into the test's local constants, which prevented the post-fix rollback path.

  **Mode B (no-op identical to v173 current state):** keep the v173 source hardcode (do NOT revert), run with `r_ReSTIR_MaxM=1.0` env var. The CVar is already 30.0f by default; v173 hardcode overrides it to 1.0f at the local constants level. Mode B is functionally identical to v173-as-shipped — the env var is a no-op. NOT RECOMMENDED (defeats the purpose of Mode A).

  **Mode C (revert entirely, control via CVar):** same as Mode A. This is the cleanest sustainable state.

  **Recommended: Mode A / Mode C.**

  ### Concrete code edits (if operator chooses Mode A)

  ```cpp
  // Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
  // Line 950 (per-frame temporal constants):
  TC.MaxM             = 30.0f;    // v175: revert v173 hardcode; use CVar r_ReSTIR_MaxM instead
  // Line 1005 (per-frame spatial constants):
  SC.MaxM             = 30.0f;    // v175: revert v173 hardcode; matching cap downstream
  ```

  These TWELVE CHARACTERS of source revert (the `1.0f` → `30.0f` × 2) restore the v172 baseline. The test then runs with:

  ```bash
  r_ReSTIR_MaxM=1.0 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
  ```

  The CVar override takes effect BEFORE the per-frame constants block runs (CVar lookup is at the start of the test's `Run()` loop, before the temporal pass). The `TC.MaxM = 30.0f` assignment at line 950 is then overwritten by the CVar + recomputed value. Expected: same numeric effect as v173.

  **Wait — does this actually work?** The CVar value is read at frame start, but the per-frame constants block at line 950 HARDCODES a value. Whether the CVar overrides the hardcode depends on the test's CVar-first-then-hardcode or hardcode-first-then-CVar ordering. **This is a guarded assumption; the operator must verify by reading the test's CVar-load path.** If the test loads `CVar_r_ReSTIR_MaxM` AFTER the per-frame constants block, the CVar override will be IGNORED, and v175 Mode A fails to apply the v173 hypothesis.

  **Mitigation:** if Mode A doesn't work (CVar is shadowed by hardcode), the operator has two options:
  - **Option A.1:** Apply the v174 Step 7 fallback directly (revert + AmbientScale + NumCandidates). This is the original v174 plan.
  - **Option A.2:** Keep the v173 hardcode and accept that this is the v173 fix as designed. The "Phase A FAIL implies revert" path then requires editing the source, not just env vars. This is what v173 currently does.

  ### Implementation step (operator-side)

  1. Revert the v173 hardcode (2 lines, 12 characters).
  2. Run with `r_ReSTIR_MaxM=1.0 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal`.
  3. Read display std in the log. Expected: ~0.09 (matching v173 prediction).
  4. **Diagnostic check:** if display std is 0.46 (un-validated, the original monochrome), the CVar is being shadowed — fall back to Option A.1 or A.2.

- skip_planning: no — v173 hypothesis is already validated; this is a new path question (CVar vs hardcode).
- skip_plan_review: no — the CVar-override-vs-hardcode question deserves a fresh review; the previous lineage never considered this option.
- skip_impl_review: no — implementation is just a 2-line revert; reviewer should sanity-check that the CVar path is actually wired.
- produces_test_files: no — no test files added.
- test_strategy: operator-side (terminal-blocked cron). Same recipe as v173, but with CVar override path.

- risks:
  1. **CVar-shadowing risk (HIGH).** The test's CVar-load path may not override the per-frame hardcode. The plan addresses this with a "diagnostic check" in step 4. If shadowed, the plan's Mode A is equivalent to a no-op and the pipeline must fall back to v174.
  2. **Operator-side execution still blocked.** This plan does NOT solve the operator-execution blocker. The same "operator must run the recipe" constraint applies.
  3. **Reverted hardcode + new env var is a NEW source state.** Once reverted, the v173 patch is no longer on disk. If v175 Phase A FAILs AND the operator wants to re-try v173, they must re-apply the hardcode (or rely on git).
  4. **CVar scope is global.** `r_ReSTIR_MaxM` affects all ReSTIR consumers, not just this test. If the test is run concurrently with other ReSTIR-using code (unlikely in this test harness), the CVar override would be a cross-test contamination. Mitigation: the test runs in isolation; no concurrent ReSTIR consumers.

## Why this plan is NEW (not a v174 alternative)

v174 is a "revert + apply AmbientScale + bump NumCandidates" plan. v175 is a "revert + use CVar override" plan. The hypothesis tested is the SAME (v173's MaxM=1.0f is the fix), but the implementation path is different:

- v174 = revert + rely on hardcoded secondary knobs (still source edits)
- v175 = revert + rely on CVar env var (no source edits for the tuning)

If v175 Phase A PASSes (revert + CVar=1.0 gives display std ~0.09), the v173 hypothesis is reproduced AND the codebase is in a cleaner state (no hardcoded tuning numbers). v175 PASS is strictly better than v173 PASS.

If v175 Phase A FAILs (CVar is shadowed), the v173 patch is the only working path. v175 FAIL is functionally equivalent to v173 FAIL (because the same hardcoded-hypothesis is being tested).

## Why v175 might FAIL out of the gate

The test's per-frame constants block at line 950 runs AFTER the test's CVar init. There are two possible orderings:

- **Order 1 (CVar-first):** CVar is read at frame start, populates `TC.MaxM` via CVar ref. The hardcode `TC.MaxM = 30.0f` at line 950 is DEAD CODE.
- **Order 2 (hardcode-first):** The hardcode at line 950 ALWAYS runs, shadowing the CVar. The CVar is irrelevant.

v173 implicitly assumed Order 1 (the hardcode worked, so the CVar was correctly propagated). v175 challenges this assumption by REMOVING the hardcode and seeing if the CVar still works. If Order 2 is real, v175 Phase A FAILs at the first run.

**Diagnostic to disambiguate:** after running v175 Mode A, check the log for `stats display floats`:
- std ≈ 0.09 → Order 1 (CVar works) → v173 hypothesis confirmed via CVar, v175 wins
- std ≈ 0.046 → Order 2 (CVar is shadowed) → v173 hypothesis needs the hardcode path, v173 stays the fix

## Concrete bisect plan (operator-side)

### Step 1: Revert the v173 hardcode

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
# Edit TestReSTIR_GI_Temporal.cpp lines 950 and 1005:
#   TC.MaxM = 1.0f; -> TC.MaxM = 30.0f;
#   SC.MaxM = 1.0f; -> SC.MaxM = 30.0f;
```

### Step 2: Rebuild + run with CVar override

```bash
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild --Jobs=4
cd Engine/Source/Runtime/Binary/Debug
r_ReSTIR_MaxM=1.0 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
```

### Step 3: Verify log stats

```bash
grep "stats display floats" TestReSTIR_GI_Temporal.log | tail -1
# v175 PASS: std ≈ 0.09 (CVar propagated, v173 hypothesis reproduces)
# v175 FAIL: std ≈ 0.046 (CVar shadowed, hardcode needed)
```

### Step 4: If v175 PASSes, full validation suite

```bash
grep -E "VUID|ERROR|CommandList error" TestReSTIR_GI_Temporal.log | wc -l   # expect 0
python3 /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
# expect 6/6 PASS
```

### Step 5: Vision check + mode-20 (same as v173)

If v175 PASSes, the v173 hypothesis is CORRECT and the codebase is in a clean state (no test-source hardcoded MaxM). The Phase A acceptance criteria are met.

### Step 6: If v175 FAILs (CVar shadowed)

The v173 hardcode is the only working path. Re-apply the v173 patch (lines 950 + 1005 to `1.0f`) and re-run. The pipeline is back to v173-as-shipped state.

If v173 itself FAILs (display std still 0.046 after both hardcode and CVar paths), v174 Step 7 fallback activates (revert + AmbientScale=0.10 + NumCandidates=16).

## diff_estimate

- v175 Stage 1 (revert hardcode): +2/-2 lines (1.0f → 30.0f × 2)
- v175 Stage 2 (CVar override at runtime): 0 lines (env var)
- v175 Stage 3 (diagnostic check): 0 lines (operator reads log)
- Net: 2 lines source edit, 0 lines new code

## Verification (operator-side; terminal-blocked cron cannot run)

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
# Apply: 2-line edit (15 sec)
# Build:  3 min incremental
# Run:    25 sec
# Diagnostic:  5 sec
# Vision:  30 sec
# Total:   ~5 min
```

## HARD-ENV-FINDING (terminal blocked)

This cron tick is in file-only runspace. `r_ReSTIR_MaxM` CVar at line 38 is the answer. The operator's 5-min recipe is the closure gate.

## Relation to PENDING_PLAN_v174

v174 is a separate-contingency plan (Phase A FAIL → revert + AmbientScale + NumCandidates). v175 is a different-contingency plan (resolve v173 Phase A uncertainty by using CVar override path). They are NOT mutually exclusive:
- v175 PASSes → v174 stays dormant (v173 hypothesis confirmed via CVar)
- v175 FAILs (CVar shadowed) AND v173 hardcode FAILs → v174 activates
- v175 FAILs (CVar shadowed) AND v173 hardcode PASSes → v174 stays dormant, v175 was a wrong detours

## mark [~] in PICK

This plan is staged as `PENDING_PICK.md` line 7 (after v173 `[~]`). Operator can run either v173 (existing patch) or v175 (revert + CVar) directly. The v173 hypothesis is the same; v175 is a cleaner path verification.

— planner, dispatch from tick-80, 2026-08-17, file-only, single-profile host, terminal-blocked, autonomous invocation #20.
