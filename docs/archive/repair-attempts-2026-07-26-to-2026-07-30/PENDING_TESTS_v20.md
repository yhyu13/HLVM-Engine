# Pending Tests v20

## Test surface for v20

v20 is a single new file: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh`. The file is a bash runner script, not a C++ test. It orchestrates runs of the existing `TestReSTIR_GI_Temporal` binary and the existing `validate_restir_gi.py` validator.

The cron is file-only; all v20 verification must be parent-driven.

## Test 1: bash syntax validation

**Goal**: confirm the runner script has no syntax errors before parent runs it.

```bash
bash -n Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh && echo "SYNTAX OK"
```

Expected: `SYNTAX OK`. If the script has unbalanced quotes, missing `fi`, or other bash-syntax issues, this catches them in <1 second.

**Why this matters**: bash scripts with syntax errors fail at execution time with cryptic error messages. A 1-second syntax check before the parent commits to a 7-9 minute run saves time.

## Test 2: dry-run path resolution

**Goal**: confirm the script's `REPO_ROOT` resolution finds the project's Build.sh.

```bash
cd <repo_root> && bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh
```

Or from any directory:

```bash
bash /path/to/HLVM-Engine/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh
```

Expected (from script output): `REPO_ROOT = /path/to/HLVM-Engine` and `[rgi] Building TestReSTIR_GI_Temporal (Debug)...`. If the script bails with `ERROR: $REPO_ROOT/Build.sh not found`, the path resolution is broken.

## Test 3: build cleanliness

**Goal**: confirm the Debug build of TestReSTIR_GI_Temporal succeeds after v3-v19 patches.

Inspect `$DATA_DIR/rgi_build.log`:

- Expected: build succeeds (`Build: PASS` in evidence file).
- Expected: 0 errors. Warnings count is informational only (not a fail).
- **FAIL if**: build fails or any compile errors are present.

## Test 4: per-mode cerr fire

**Goal**: confirm the v12 default-ON cerr writes fire in both default and mode-6 runs.

Inspect the cerr-fire counts in `rgi_evidence.txt`:

- Expected: `cerr_default_render >= 8` lines (8 frames × 1 per frame) and `cerr_default_fgipass >= 8` lines (8 frames × 1 per frame).
- Expected: `cerr_mode6_render >= 1` and `cerr_mode6_fgipass >= 1` (1 frame each).
- **FAIL if**: any cerr count is 0. The v12 patch is default-ON; if cerr doesn't fire, the stderr stream is being intercepted by the test harness, or the binary on disk doesn't have v12's patch.

## Test 5: per-mode dump presence

**Goal**: confirm each mode's run produces PNG dumps.

Inspect `rgi_evidence.txt`:

- Expected: `default: <count> PNGs` where count >= 1 (the validator reads `*frame8.png`).
- Expected: `mode6`, `mode7`, `mode8`, `mode9`, `mode10`, `mode11`, `mode12`, `mode15`, `mode99: <count> PNGs` where count >= 1 each.
- **FAIL if**: any mode's dump count is 0. PNGs missing means HLVM_DUMP_RGI env var wasn't picked up, or the test exited before reaching the dump site.

## Test 6: validator verdict

**Goal**: confirm the validator's 3-check structural test on default-mode dumps.

Inspect `rgi_validator.log`:

- Expected: `3/3 checks PASSED`.
- Expected: per-check lines:
  - `Best non-black channel mean: <something> mean=... PASS`
  - `display_frame8 std=<X>, threshold=30.00 -> PASS`
  - `4x4 cell-mean std=<X>, threshold=8.00 -> PASS`
- **FAIL if**: any check returns FAIL, OR the validator exits with non-zero.

## Test 7: vision analysis (parent-driven, requires vision tool)

**Goal**: confirm each mode's dump is the expected diagnostic shape.

For each mode's `gi_raw_frame*.png` (in `$DATA_DIR/dumps_<mode>/`), vision-analyze and verify:

- **mode 0 (default)**: recognizable non-uniform Sponza geometry. If uniform color, the renderer is broken.
- **mode 6**: per-pixel gradient in R and B, G=0. Sentinel-probe shape.
- **mode 7**: scene-shape × 1.5 (should match mode 1 × 1.5 × AmbientColor.rgb × ambientScale).
- **mode 8**: TraceRay-only result; should be a clean frame if RT is healthy. If all-NaN, RT setup is broken.
- **mode 9**: diffuse-only (mode 1 × 1.5).
- **mode 10**: red channel value = `g_GI.Params5.x / 256.0`. Should be ~0.04 for debugMode=10.
- **mode 11**: gray value = `g_View.FrameIndex / 256.0`. Should be non-zero.
- **mode 12**: `(1, 1, 1)` per pixel (since AmbientColor = (1,1,1,1)).
- **mode 15**: gray value = `g_GI.Params5.x`. Should be 10.0 for debugMode=15.
- **mode 99 (default-case)**: gray `(0.5, 0.5, 0.5)`. Catch-all sentinel.

**Decision matrix after vision analysis**:

- **All probes match expectations + mode 0 gi_raw non-zero + display correct + validator 3/3** → bug is in payload/result merge or accumulate/ReSTIR/denoise passes. Cron routes to v21 (investigate accumulate / ReSTIR / bilateral denoise pass for any writes to OutputTexture after the GI dispatch).
- **mode 6 works but mode 12 fails** → bug is in AmbientColor uniform bind.
- **mode 6/7 work but mode 8 crashes** → bug is in TraceRay's interaction with payload.
- **mode 6/7/8/9 all 0 + default works** → slangc dead-strip confirmed; investigate debugMode switch compilation.
- **mode 6/7/8/9 all 0 + default also 0** → switch not entered at all; investigate `debugMode != 0u` guard or cbuffer reach.
- **mode 10 = 0 but mode 15 = 15.0** → divide-by-256 issue.
- **mode 11 = 0** → View cbuffer not bound.

## Test 8: regression carryover (post-fix)

After a renderer fix lands, re-run `run_rgi_diagnostic.sh` and verify:

- Build: PASS
- cerr fires: PASS
- mode 0 gi_raw: shows Sponza (not garbage)
- mode 0 validator: 3/3 PASS
- All other modes: behave per Test 7 expectations

**Acceptance criteria for completion** (carried over from cron prompt):

1. Debug target builds
2. Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — script produces this for mode 0
3. No command-list-already-open errors — verify in `rgi_default.stderr`
4. No Vulkan ERROR/VUID in fresh log — verify in `rgi_default.stderr`
5. Validator passes newest dump group — verify in `rgi_validator.log`
6. Display visibly contains recognizable non-uniform Sponza — verify via vision on `dumps/display_frame*.png`
7. Relevant checks pass (mean luma + spatial std + cell variance) — verify via validator

If all 7 acceptance criteria are met after a v21+ fix cycle, write `docs/PIPELINE_GOAL_DONE_2026-07-27.md` and mark the v0 task `[x]` in PENDING_PICK.md.

## Test 9: cleanup verification

After each successful test run, confirm:

- `$DATA_DIR/dumps/` contains only the default-mode dumps (script moves `dumps_<mode>` subdirs to `dumps_<mode>/` to keep them separate).
- `$DATA_DIR/dumps_<mode>/` exists for each mode run.
- `$DATA_DIR/rgi_*.log` and `$DATA_DIR/rgi_*.stderr` and `$DATA_DIR/rgi_*.stdout` files exist for each mode.
- `$DATA_DIR/rgi_evidence.txt` is the consolidated summary.

If any of these are missing, the script's cleanup phase has a bug.

## Test 10: idempotency

**Goal**: confirm running the script twice produces two independent evidence files (not overwriting the first).

```bash
mv $DATA_DIR/rgi_evidence.txt $DATA_DIR/rgi_evidence_run1.txt
bash run_rgi_diagnostic.sh
diff <(head -20 rgi_evidence_run1.txt) <(head -20 rgi_evidence.txt)
# Expect: timestamp differs, structure identical
```

If `rgi_evidence.txt` gets overwritten each run, the file should be renamed to include timestamp. (Not currently a bug; rgi_evidence.txt is meant to be the latest run's evidence.)

## Verifier verdict target

- **ALL_KEEP** if Tests 1-9 pass and Test 10 demonstrates script idempotency.
- **SOME_RELAX** if Tests 1-6 pass but Tests 7-10 cannot be verified by parent within the cron tick (terminal blocked).

**Expected verdict for v20**: SOME_RELAX. The cron cannot execute Tests 1-10 itself; they're all parent-driven. The script's existence on disk is the deliverable; the parent's execution is the verification.