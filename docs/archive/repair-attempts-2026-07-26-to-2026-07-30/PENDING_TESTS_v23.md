# Pending Tests v23

## Test surface for v23

v23 is a script-fix cycle: it patched `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh` to fix the off-by-one dump-rotation bug. No source code (C++ / HLSL / CMake) was modified.

This PENDING_TESTS_v23.md documents BOTH:
1. Tests for the v23 script-fix cycle (verifying the patched script is correct by static inspection and parent-driven execution).
2. Tests for the v22 evidence path (which depends on the fixed script producing correctly-labeled per-mode dumps).

The cron is file-only; all test execution is parent-driven.

## Part A: v23 script-fix tests (immediately exercisable via static inspection)

### Test A1: archive-after-run pattern

**Goal**: confirm each iteration's archive is created AFTER the test run, naming the archive with the mode that produced the output.

```bash
grep -n -A2 "Archive AFTER the run" Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh
```

Expected: lines 113-121 contain the post-run archive block:
```
# Archive AFTER the run: dumps_${mode_name} now contains THIS mode's output.
# If the run failed (empty dumps), the archive is empty too — that is the
# correct evidence (zero PNGs for a failed mode). The previous v20 logic
# destroyed mode99's output here by moving it to dumps_default on the
# post-loop restore; the archive-after-run pattern avoids that destruction.
if [ -d "$DATA_DIR/dumps" ]; then
  rm -rf "$DATA_DIR/dumps_${mode_name}" 2>/dev/null || true
  mv "$DATA_DIR/dumps" "$DATA_DIR/dumps_${mode_name}" 2>/dev/null || true
fi
```

### Test A2: pre-loop archive of stale dumps

**Goal**: confirm stale pre-run `dumps/` is moved to `dumps_prerun` before the first iteration.

```bash
grep -n -A4 "Stale pre-run dumps" Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh
```

Expected: lines 81-88 contain the pre-loop archive block with `dumps_prerun` destination.

### Test A3: post-loop validator restoration

**Goal**: confirm the post-loop restoration uses `cp -r` with `mv` fallback (preserves the archive).

```bash
grep -n -A4 "Restore dumps/ to the DEFAULT mode" Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh
```

Expected: lines 124-131 contain `cp -r "$DATA_DIR/dumps_default" "$DATA_DIR/dumps" 2>/dev/null || \` and `mv` fallback.

### Test A4: absence of buggy pre-run rotation

**Goal**: confirm the buggy v20 pattern (`mv "$DATA_DIR/dumps" "$DATA_DIR/dumps_${mode_name}"` BEFORE the run, not after) is gone.

```bash
grep -n "Move existing dumps to a per-mode archive" Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh
```

Expected: NO MATCH. The buggy comment is gone.

### Test A5: header comment v23 attribution

**Goal**: confirm the script header documents the v23 fix.

```bash
grep -n "v23 (six-role-pipeline, 2026-07-27)" Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh
```

Expected: lines 26-30 contain the v23 attribution block.

### Test A6: bash syntax validity

**Goal**: confirm the patched script is syntactically valid bash.

Run (parent-driven, since cron terminal is blocked):
```bash
bash -n Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh
```

Expected: exit 0, no syntax errors.

## Part B: v22 evidence-path tests (parent-driven, after v23 is applied AND script is re-run)

These tests apply if/when the parent runs the patched `run_rgi_diagnostic.sh` and gets correctly-labeled per-mode dumps.

### Test B1: per-mode dump counts (off-by-one resolution)

**Goal**: confirm each per-mode archive contains exactly the dumps for the mode that PRODUCED it (not the previous mode's output).

Run (parent-driven):
```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh

# After the script completes:
for mode in default mode6 mode7 mode8 mode9 mode10 mode11 mode12 mode15 mode99; do
  count=$(ls "Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps_$mode"/*frame*.png 2>/dev/null | wc -l)
  echo "$mode: $count PNGs"
done
```

Expected (assuming each mode produces 3 PNGs — gbuffer_material, gbuffer_normal, gbuffer_worldpos — per the stale dump group's pattern):
```
default: 3 PNGs
mode6: 3 PNGs
mode7: 3 PNGs
mode8: 3 PNGs
mode9: 3 PNGs
mode10: 3 PNGs
mode11: 3 PNGs
mode12: 3 PNGs
mode15: 3 PNGs
mode99: 3 PNGs
```

vs the BUGGY v20 behavior:
```
default: 3 PNGs (correct by coincidence — iter 2 overwrites iter 1)
mode6: 3 PNGs (actually default's PNGs — off-by-one)
mode7: 3 PNGs (actually mode6's PNGs — off-by-one)
...
mode99: 0 PNGs (destroyed by post-loop restore)
```

### Test B2: mode99 dump preservation

**Goal**: confirm `dumps_mode99/` is non-empty (currently destroyed by the buggy v20 logic).

```bash
ls -la Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps_mode99/ | head
```

Expected: 3 PNGs present (mode99's frame8 outputs).

### Test B3: rgi_evidence.txt per-mode counts

**Goal**: confirm `rgi_evidence.txt` reports correct per-mode dump counts.

```bash
grep -A20 "Per-mode dump presence" Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/rgi_evidence.txt
```

Expected: 10 lines, each showing `3 PNGs` (vs the buggy v20 output: 9 lines, mode99 showing 0 PNGs and all per-mode labels being off-by-one).

### Test B4: rgi_evidence.txt cerr counts unchanged

**Goal**: confirm the per-mode cerr and spdlog counts are unchanged by the v23 patch (the rotation bug only affects dumps, not stderr/stdout capture).

```bash
grep -E "Cerr fire check|v3 spdlog markers" Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/rgi_evidence.txt
```

Expected: same shape as before, but per-mode (the stderr/stdout capture is keyed on `rgi_${mode_name}.stderr` which is correct in both versions).

### Test B5: validator still runs on default-mode dumps

**Goal**: confirm the post-loop `cp -r dumps_default dumps/` makes `dumps/` contain the default-mode output (which the validator expects).

```bash
ls Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/*frame*.png | wc -l
```

Expected: 3 PNGs (the default-mode frame8 outputs).

### Test B6: parent decision routing

**Goal**: after the patched script produces correctly-labeled dumps, parent pastes `rgi_evidence.txt` back to the cron. The cron's v22 routing then resolves correctly based on the per-mode evidence (instead of being misled by off-by-one dump labels).

This is a meta-test: the cron routes to v21a (binding-layout-split fix) if hypothesis #1 (nvrhi-deferred-barrier-ordering) is confirmed by the correctly-labeled evidence. The previous v20 script would have routed to the wrong v21b..v21i branch if the off-by-one was misleading the parent's evidence interpretation.

## Test verdict target

- **ALL_KEEP** for Part A: Tests A1-A6 are static (Test A6 requires shell; cron can verify A1-A5 by `search_files`/`read_file`).
- **SOME_RELAX** for Part B: Tests B1-B6 are parent-driven; the cron cannot execute them. After the patched script is run, the parent must verify Tests B1-B6 to confirm the fix at runtime.

**Expected verdict for v23**: SOME_RELAX (Part A is verifiable in this tick; Part B is gated on parent re-running the fixed script).

## Verification recipe (parent-driven, after v23 is applied)

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh
```

This single command:
1. Builds TestReSTIR_GI_Temporal
2. Runs default mode + modes 6/7/8/9/10/11/12/15/99 (each producing correctly-labeled per-mode dumps after v23's fix)
3. Runs the validator on default-mode dumps
4. Emits `rgi_evidence.txt` with consolidated results

The parent pastes `rgi_evidence.txt` back to the cron. The cron then routes to v22 (v21a binding-layout-split fix) if hypothesis #1 is confirmed, or to v21b..v21i (one of 8 alternative sub-plans) if a different branch is confirmed.