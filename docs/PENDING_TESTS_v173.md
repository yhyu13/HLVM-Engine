# Pending Tests v173

- plan: docs/PENDING_PLAN_v173.md
- plan_review: docs/PENDING_PLAN_REVIEW_v173.md
- commit: docs/PENDING_COMMIT_v173.md
- impl_review: docs/PENDING_IMPL_REVIEW_v173.md
- produces_test_files: no — the patch is a constant-tuning edit (2 lines in TestReSTIR_GI_Temporal.cpp); no new test files produced; existing `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` exercises the temporal/spatial passes implicitly through dump inspection
- tester: tester (file-only this tick; cron has terminal blocked by tirith)
- timestamp: 2026-08-16T-tick-now-Z

## Test strategy (from PENDING_PLAN_v173.md §test_strategy)

**Operator-side runspace required.** Cron this tick is in a file-only runspace (`terminal` blocked at tirith security-pattern gate, cumulative ≥1772+ denials on this lineage; this turn probes also denied). The recipe in `PENDING_PLAN_v173.md` §"Concrete bisect plan" lines 94-150 is the test contract; the operator executes it.

The plan already enumerates 7 acceptance criteria; the cron can only audit PATCH FIDELITY (DONE in IMPL_REVIEW_v173), not RUNNING behavior.

## File-only test verification (this tick)

The patch IS applied on disk (verified by direct read_file of `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` lines 950 and 1005):
- Line 950: `TC.MaxM = 1.0f;     // v173: small M → W≈1 → preserve per-pixel variance` (was `30.0f`)
- Line 1005: `SC.MaxM = 1.0f;     // v173: matching cap downstream of temporal` (was `30.0f`)

The binary `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log` (the active log file, last touched 2026-08-14 22:18:56) has **0 VUIDs** per `search_files content count` (verified this tick). The binary on disk is post-v166+v168+v169 baseline (clean). But the binary does NOT include the v173 MaxM change yet — the operator must run `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild` to incorporate the patch.

## Operator-side test recipe (the binding contract)

Per `PENDING_PLAN_v173.md` §"Concrete bisect plan" steps 2-6 (verbatim, single-source-of-truth):

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine

# Patch IS already on disk (verified file-only this tick); rebuild picks it up
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild

# Run + dump
cd Engine/Source/Runtime/Binary/Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal

# Verify log stats
grep "stats display floats" TestReSTIR_GI_Temporal.log | tail -1     # expect std >= 0.09
grep "stats gi_raw floats"  TestReSTIR_GI_Temporal.log | tail -1     # expect std >= 0.09
grep -E "VUID|ERROR|CommandList error" TestReSTIR_GI_Temporal.log | wc -l   # expect 0

# Run validator
python3 /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
# Expect: 6/6 PASS (or fewer failures pre-fix → more passes post-fix)

# Vision check (operator-side, vision_analyze tool or local image viewer)
ls -t Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/*display_frame8.png | head -1
# Expect: recognizable Sponza gallery arches + floor + back wall + directional shadow

# Mode-20 sanity (the user's specified discriminator)
HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
# Expect gi_raw dump is NON-UNIFORM (per-pixel albedo color, not solid zero or constant mid-gray)
```

**Total operator-side effort**: ~5 min for incremental build + ~25 sec for run + ~30 sec for grep/validate/vision.

## Why this cron tick is a tester cycle-stop

The test contract (the 5-min recipe above) requires terminal+python3+vision execution. The cron runspace is blocked at tirith. Per `six-role-pipeline §When NOT to use this skill`:

> 1. The work is **interactive debugging** — read code, run test, look at dump, form hypothesis, repeat in 5 min.
> 2. The fix is a **single-line surgical patch**.
> 3. The host has **only one worker profile** AND the work requires real fresh-eyes review.

ALL THREE apply here. The skill's own guidance is the **blocker branch** the user explicitly authorized ("or report concrete external blocker with evidence").

## Cron capability-vs-permission table (tester role)

| Action | Capability | Permission | Result |
|--------|-----------|-----------|--------|
| Verify patch on disk | yes | yes | DONE (`TC.MaxM=1.0f`, `SC.MaxM=1.0f` confirmed by read_file) |
| Build target | yes | **no** (tirith blocks terminal) | NOT DONE |
| Run target | yes | **no** | NOT DONE |
| Grep log stats | yes | **no** (terminal) | NOT DONE (cron can read file-only via read_file, but no fresh log exists post-rebuild) |
| Run validator | yes | **no** | NOT DONE |
| Vision analyze PNG | yes | **no** | NOT DONE |

## Honest verdict

The patch is ON DISK. The recipe is COMPLETE and TESTABLE. The CRON cannot execute the test — only the operator can. This tester marker is a contract-handoff, not a test result.

When the operator runs the recipe and reports back:
- If `display std >= 0.09` AND `validate_restir_gi.py` exits 0 AND vision shows Sponza → v173 is KEEP (PASS); the bisect is closed; PICK line 118 can be marked `[x]`
- If `display std < 0.09` OR validator fails OR vision shows monochrome → v173 is FIX; revert + apply compound fix per plan Step 7
- If `0 < display std < 0.07` AND validator partially passes → v173 PARTIAL; review per audit verdict

— tester, 2026-08-16, tick-2026-08-16, file-only, single-profile host, terminal-blocked.