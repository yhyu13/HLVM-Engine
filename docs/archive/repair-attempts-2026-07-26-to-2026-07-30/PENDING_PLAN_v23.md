# Pending Plan v23 — Fix the off-by-one dump-rotation bug in `run_rgi_diagnostic.sh`

- task: surgical fix to the dump-rotation logic in `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh` lines 88-92. The current code moves `dumps/` to `dumps_${mode_name}` BEFORE each run, which causes an off-by-one mislabeling: each mode's actual output lands in the NEXT mode's archive directory, and the validator (which runs against the directory named `dumps_default`) ends up validating against STALE pre-run dumps, not the actual default-mode output. The last mode's output (mode99) is destroyed when the post-loop restoration overwrites `dumps/`.
- source: file-only patch to one shell script (`run_rgi_diagnostic.sh`). No C++ / HLSL / CMake source touched. No pipeline markers outside `docs/` touched.
- approach:
  1. **Root-cause trace**: at script line 88-92, `mv "$DATA_DIR/dumps" "$DATA_DIR/dumps_${mode_name}"` archives the CURRENT contents of `dumps/` (which are the prior iteration's mode output, except in the first iteration where it's pre-existing stale data) under the CURRENT iteration's mode name. So:
     - Iter 1 (default): `dumps/` (stale) → `dumps_default`; run fills `dumps/` with default output.
     - Iter 2 (mode6): `dumps/` (default output) → `dumps_default` (overwrites stale; default output now correctly in `dumps_default`); run fills `dumps/` with mode6 output.
     - Iter 3 (mode7): `dumps/` (mode6 output) → `dumps_mode6`; run fills `dumps/` with mode7 output.
     - ...continues correctly from iter 3 onwards.
     - Iter 10 (mode99): `dumps/` (mode98 output) → `dumps_mode98`; run fills `dumps/` with mode99 output.
     - After loop: `dumps/` contains mode99 output. `dumps_mode99` does NOT exist.
  2. **Wait — re-examining the watchdog's claim more carefully**: The watchdog said "fresh mode99 is deleted" by lines 106-111. Let me re-trace:
     - Iter 10 END: `dumps/` contains mode99 output. Loop exits.
     - Lines 108-111: `if [ -d "$DATA_DIR/dumps_default" ]; then rm -rf "$DATA_DIR/dumps"; mv "$DATA_DIR/dumps_default" "$DATA_DIR/dumps"; fi`.
     - This removes `dumps/` (which contained mode99) and replaces it with `dumps_default`.
     - So mode99's output is destroyed, and `dumps/` contains `dumps_default` (default mode output, correctly archived at iter 2 overwrite).
     - But on iter 1, `dumps_default` received STALE not default. Then iter 2 OVERWROTE `dumps_default` with the actual default output. So `dumps_default` after the loop = default mode output. ✓
     - The watchdog's claim "stale pre-run dumps/ becomes dumps_default, fresh default output becomes dumps_mode6" — actually that's NOT what happens because iter 2 OVERWRITES `dumps_default`. So `dumps_default` is correctly the default mode's output after the loop.
  3. **The ACTUAL bug**: every mode's archive (`dumps_mode6`, `dumps_mode7`, ..., `dumps_mode98`) contains the PREVIOUS mode's output, not its own. So when the user inspects `dumps_mode6/`, they see mode5 output (which doesn't exist in the loop — mode5 was never run, so `dumps_mode6/` contains default output, NOT mode6 output). Off-by-one across the board. The mode99 output is destroyed by lines 108-111.
  4. **The user-visible failure mode**: the per-mode dump-presence check at lines 154-162 reads `dumps_default` and `dumps_<mode_name>` for each iteration. `dumps_default` IS the actual default output (after iter 2's overwrite). But `dumps_mode6` IS default output (mislabeled). `dumps_mode7` IS mode6 output. ... `dumps_mode99` does not exist (mode99 was destroyed). And `dumps_mode6` through `dumps_mode98` are all mislabeled by one slot.
  5. **The validator runs against `dumps/` after restoration** (lines 106-111). The restored `dumps/` IS the actual default mode output (after iter 2's overwrite). So the validator's input IS correct, contrary to the watchdog's claim.
  6. **HOWEVER — the validator receives only the DEFAULT mode output**. The diagnostic's whole point is to compare per-mode dumps (modes 6/7/8/9/10/11/12/15/99) to identify which mode's expectations match. If `dumps_mode6` contains default output, then mode6's "per-pixel gradient" expectations are silently compared against the DEFAULT output (which is what mode6 would have looked like if the switch didn't fire). The user would see mode6's stderr log + `dumps_mode6/` PNG and conclude mode6 matches default output → "mode 6 silent = switch not entered" — possibly wrong conclusion because the `dumps_mode6/` PNG is actually default output, not mode6 output.
  7. **Fix design**: move AFTER the run, not before. Use the mode name that PRODUCED the dumps as the archive label.
- diff_estimate: +5 / -5 lines (the same 5-line block reorganized; net 0 lines if I keep the surrounding structure). If I add a helper function for clarity, +12 / -5 = +7 lines.
- skip_plan_review: no — the bug is in the user-facing evidence-collection script, so correctness is high-stakes (parent's v22 decision depends on it).
- test_strategy:
  1. Static test (cron, file-only): re-read the patched script and confirm the archive-after-run pattern.
  2. Parent-driven test: parent runs `bash run_rgi_diagnostic.sh` and confirms:
     - `dumps_default/` contains 3 frame8 PNGs (matching the test's per-run dump pattern).
     - `dumps_mode6/` through `dumps_mode99/` each contain 3 frame8 PNGs (one per run).
     - `dumps_mode99/` exists and is NOT empty (currently destroyed by lines 108-111).
     - `rgi_evidence.txt` reports correct per-mode dump counts (currently: 10 PNGs in default dir, 0 in mode99 dir).
- risks:
  - **Risk A: the first iteration's pre-existing `dumps/` may contain unrelated stale dumps.** If parent's repo has stale dumps from prior runs, the first iteration's move (now AFTER the first run, so it's the default output that gets moved, not the stale) — but on iter 1, there's nothing to archive. The pre-existing stale `dumps/` should be cleared before the loop starts. Add a `rm -rf "$DATA_DIR/dumps"` before the loop, then `mkdir -p` inside the loop.
  - **Risk B: per-mode run failure should not abort the loop.** Currently the loop uses `|| true` so failed runs continue. After the fix, a failed run produces an empty `dumps/` which gets archived as `dumps_modeN/` (empty). This is correct behavior (empty archive means the mode failed).
  - **Risk C: parent already ran the script once and has mislabeled archives.** They should `rm -rf dumps_*` before re-running. Document this in the script header.
  - **Risk D: the watchdog's analysis was wrong about validator input.** The validator does receive correct default-mode output (after iter 2's overwrite). The fix is to make the per-mode archives ALSO correct. The validator was already working — the per-mode diagnosis was broken.

## Why this cycle is correct

The outer-watchdog heartbeat at `docs/PIPELINE_HEALTH_2026-07-27_outer_v24.md` flagged this as a defect in the v20 runner:

> "v20's `run_rgi_diagnostic.sh` mis-rotates dumps: each loop archives the previous run under the current mode, so stale pre-run `dumps/` becomes `dumps_default`, fresh default output becomes `dumps_mode6`, later modes remain shifted, fresh mode99 is deleted, and lines 106–111 restore stale `dumps_default` for validation."

The watchdog's diagnosis is correct in the BROAD STROKES (off-by-one rotation; mode99 output destroyed) but INCORRECT in one specific detail: `dumps_default` after the loop IS the actual default-mode output (because iter 2's `mv "$DATA_DIR/dumps" "$DATA_DIR/dumps_default"` overwrote the stale iter-1 archive with the real default output).

But the per-mode dump-presence check at lines 154-162 reports the wrong mode's output for every iteration 3-10, AND mode99's output is destroyed by lines 108-111. The user inspecting `dumps_mode7/` thinking it's mode7's output actually sees mode6's output. The diagnostic verdict derived from inspecting `dumps_<mode>/` PNGs would be systematically wrong.

The v22 PICK item is gated on parent running `run_rgi_diagnostic.sh` AND `rgi_evidence.txt` confirming hypothesis #1. If the script's per-mode archives are wrong, the parent's evidence-shape analysis is unreliable → wrong branch selected → wrong fix applied. The fix is mechanical and surgical, and the cron's "continue cycling" instruction authorizes it.

The fix also does NOT depend on terminal access (file-only patch) and does NOT require parent's intervention to verify in static form. Parent-driven execution is required to confirm the fix at runtime, but the static correctness is verifiable by re-reading the patched script.

## Decision tree

This is the only branch — the bug is identified, the fix is mechanical, no alternative interpretation is viable. Single-cycle execution.

## Implementation outline

```bash
# In run_rgi_diagnostic.sh, replace lines 81-93 with:

mkdir -p "$DATA_DIR/dumps"

# If a previous run left a stale dumps dir, archive it so it doesn't pollute this run.
if [ -d "$DATA_DIR/dumps" ] && [ -n "$(ls -A "$DATA_DIR/dumps" 2>/dev/null)" ]; then
  mv "$DATA_DIR/dumps" "$DATA_DIR/dumps_prerun" 2>/dev/null || true
  mkdir -p "$DATA_DIR/dumps"
fi

for mode_name in "${MODE_NAMES[@]}"; do
  mode_num="${MODE_NUMS[$mode_name]}"
  echo ""
  echo "=== Phase 2.${mode_name}: mode=$mode_num ==="

  # Fresh dumps dir for this mode's run.
  rm -rf "$DATA_DIR/dumps" 2>/dev/null || true
  mkdir -p "$DATA_DIR/dumps"

  cd "$BIN_DIR"
  echo "[rgi] Running HLVM_PT_DEBUG_MODE=$mode_num HLVM_RGI_ACCUM=$([ "$mode_num" = "0" ] && echo 8 || echo 1) ..."
  if [ "$mode_num" = "0" ]; then
    HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal \
      > "$DATA_DIR/rgi_${mode_name}.stdout" 2> "$DATA_DIR/rgi_${mode_name}.stderr" || true
  else
    HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=$mode_num HLVM_RGI_ACCUM=1 ./TestReSTIR_GI_Temporal \
      > "$DATA_DIR/rgi_${mode_name}.stdout" 2> "$DATA_DIR/rgi_${mode_name}.stderr" || true
  fi

  # Archive THIS mode's output under ITS OWN name (the off-by-one fix).
  # If the run failed (empty dumps), archive the empty dir anyway so the
  # evidence table shows a 0-count for the failed mode.
  if [ -d "$DATA_DIR/dumps" ]; then
    rm -rf "$DATA_DIR/dumps_${mode_name}" 2>/dev/null || true
    mv "$DATA_DIR/dumps" "$DATA_DIR/dumps_${mode_name}" 2>/dev/null || true
  fi
done

# Restore dumps/ to the DEFAULT mode's output for the validator.
if [ -d "$DATA_DIR/dumps_default" ]; then
  cp -r "$DATA_DIR/dumps_default" "$DATA_DIR/dumps" 2>/dev/null || \
    (rm -rf "$DATA_DIR/dumps" && mv "$DATA_DIR/dumps_default" "$DATA_DIR/dumps") || true
fi
```

Key changes from the buggy v20:
- **Lines 81-93 (pre-loop + pre-iter archive)** → replaced with archive of stale pre-run dumps to `dumps_prerun` (so the first iteration's output isn't confused with prior run data)
- **Inside loop (after the run, after the `mv` archive)** → archive `dumps/` → `dumps_${mode_name}` AFTER the run, so the archive name reflects the mode that produced it
- **Lines 106-111 (post-loop restoration)** → use `cp -r` with `mv` fallback so mode99's archive is not destroyed if it happens to be the last archive referenced (it's not in this case, but `cp -r` is safer)

## Files

- Modify: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh` (lines 81-93 + lines 106-111 replaced with archive-after-run pattern; net +7 lines)

## Verification (parent-driven, after v23 is applied)

1. Run `bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh`
2. Verify per-mode dump presence:
   ```bash
   for mode in default mode6 mode7 mode8 mode9 mode10 mode11 mode12 mode15 mode99; do
     echo -n "$mode: "; ls "$DATA_DIR/dumps_$mode"/*frame*.png 2>/dev/null | wc -l
   done
   ```
   Expected: 3 PNGs per mode (default outputs `display_frame8`, `gi_raw_frame8`, plus the test's primary frame; non-default modes output the same pattern with the per-pixel-gradient PNG)
3. Verify `rgi_evidence.txt` shows correct per-mode dump counts (currently buggy: mode6 shows default's PNGs, mode99 shows 0)
4. Verify `dumps_mode99/` is NOT empty (currently destroyed by lines 108-111)
5. Re-inspect the per-mode PNGs to confirm the off-by-one is gone

## Notes for impl-reviewer

- The fix is purely shell-script logic. No C++ / HLSL / CMake changes.
- The fix does NOT depend on terminal access. The cron can apply it file-only via `patch`.
- The fix does NOT change the per-mode stderr/stdout capture (those remain per-mode-named via `rgi_${mode_name}.{stdout,stderr}`).
- The fix does NOT change the validator invocation or the evidence summary composition.
- The fix is fully reversible: `git checkout run_rgi_diagnostic.sh` restores the buggy v20 version.
- The watchdog's analysis was broadly correct but had one detail wrong (`dumps_default` IS correct after iter 2 overwrite). The fix addresses both the broad-stroke and the detail-level correctness.