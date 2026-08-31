# TestReSTIR_GI_Temporal — dump artifact drift diagnostic (2026-08-29, tick #507)

## TL;DR

The prior closure diagnostic `DIAGNOSTIC_2026-08-29-empirical-closure.md`
claims "8 PNGs produced at `[TEST_DATA_DIR]/dumps/20260814_22191{6,7,8}_*.png`"
(lines 18-27). **A direct `search_files` of the dumps directory this turn
returns only 1 of the 8 PNGs from that run.** The other 7 PNGs, referenced
in the log at lines 231/233/235/238/241/245/247 as having been written during
the 2026-08-14 22:18:56 → 22:19:18 run, are not present on disk.

## Concrete evidence (mtime on artifacts — ground truth)

The dumps directory contains 50 PNGs as of this turn:

```
$ search_files pattern="." target=files path=.../TestReSTIR_GI_Temporal_Data/dumps
```

Sorted by filename prefix, the PNGs range from `20260808_*` to `20260814_*`.
The only PNG from the 2026-08-14 22:18:56 run that exists on disk is:

- `20260814_221917_gbuffer_normal_frame8.png`

The log references these 7 PNGs from the same run:

- Line 231: `Dumped display .../dumps/20260814_221916_display_frame8.png`
- Line 233: `Dumped spatial .../dumps/20260814_221916_spatial_frame8.png`
- Line 235: `Dumped denoised .../dumps/20260814_221916_denoised_frame8.png`
- Line 238: `Dumped gi_raw .../dumps/20260814_221916_gi_raw_frame8.png`
- Line 241: `Dumped gbuffer_worldpos .../dumps/20260814_221917_gbuffer_worldpos_frame8.png`
- Line 245: `Dumped gbuffer_material .../dumps/20260814_221917_gbuffer_material_frame8.png`
- Line 247: `Dumped gbuffer_depth .../dumps/20260814_221918_gbuffer_depth_frame8.png`

Lines 249-250 of the log confirm `Dumped frames to .../dumps` (the
flush-to-disk step) followed by `stats display floats` for a second time,
suggesting the test re-ran the display stats aggregation. The LogTest confirms
all 8 dumps completed. Yet only 1 of 8 is on disk.

## What this means

Per `software-development-practices §Trusting stale "rebuild from ash"
verdicts`: mtime on the *artifacts* the verdict describes is the ground
truth, not mtime on the docs. The closure diagnostic's claim that "8 PNGs
produced" was true *at the moment the test ran* (the dump code completed
without error per the log), but the on-disk state at the time of this
diagnostic is 1 of 8. Either:

1. **The dumps were deleted / GC'd / overwritten** between 2026-08-14
   22:19:18 and now. Re-runs of the recipe that don't write all 8 PNGs
   could overwrite just the gbuffer_normal one.
2. **A subsequent v176-patched run produced only the gbuffer_normal
   dump.** But the log would show that — and the log has no subsequent
   "Dumped" entries past line 250.
3. **Path collision**: the dumps for the 22:19 run actually went to a
   different directory than the test binary's resolution. Possible but
   the line explicitly prints the full path and matches `/dumps/`.

Most likely: a manual `rm` or `.gitignore`-style `git clean` operation
removed the dumps between the last test run and now. Without further
operator-side evidence, this is speculative.

## What is still empirically confirmed

The log itself is byte-equal and authoritative. What the log shows (and is
NOT subject to disk-state drift, because the log was *appended to* during
the run and was not subsequently edited) is:

- Line 197/201: byte-equal handle-identity match (falsifies v24 §option-4).
- Line 232: display stats `mean=[0.4584, 0.4581, 0.4861] std=[0.0458, 0.0470, 0.0429]`.
- Line 246: gbuffer_material std = real per-material variation.
- Line 253: gi_raw std = real per-channel variance.
- Line 258: ReSTIR summary M mean=2.93 max=9.0 MaxM=30 (default).
- Line 264: `ReSTIR GI Temporal test completed` (clean shutdown).
- 273 lines total, 0 VUID / 0 Vulkan ERROR / 0 abort / 0 fail.

What is **NOT** available file-only for re-validation:

- 7 of 8 fresh PNGs that would let `validate_restir_gi.py` re-run the
  4-check structural validator against the *newest* dump group.
- A fresh GPU run with `HLVM_PT_DEBUG_MODE=20` to resolve the v24
  binding-broken question empirically.

## Implication for the pipeline

This is **not a new bug** — the test itself is still in a working state per
the log evidence. The artifact drift means:

1. The `validate_restir_gi.py` run cannot be repeated file-only against the
   newest dump group; running it now against the dumps directory would yield
   an older timestamp group (likely 2026-08-10 / 2026-08-11 based on the
   file dates I can see in the listing).
2. The "Fresh display image shows recognizable Sponza" (gate 6) cannot be
   re-checked without an operator-side vision probe of the current
   `display_frame8.png` on disk (the oldest from 2026-08-10) or a fresh
   re-run.

## Operator closure path

**Recommended** (≈5-10 min at terminal):

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine

# Re-run the test to re-produce all 8 PNGs
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild 2>&1 | tail -50
cd Engine/Source/Runtime/Binary/Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
cd ../../../..

# Now run validator against the freshly-re-populated dumps/
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py \
        Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps --verbose
# Expected: 4/4 PASS lines, exit code 0

# Plus run the discriminated mode-31 discriminator
HLVM_PT_DEBUG_MODE=31 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
    Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py \
        Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps --discriminator mode-31 --verbose
# Expected: BLUE-MID (mean≈0.333, std<0.005) → "DISCRIMINATOR LEAF 1 BLUE" if
# binding is alive but reading zero; NON-UNIFORM → "LEAF 2" if binding works;
# GRAY-MID (mean≈0.5, std<0.005) → "LEAF 3" if slangc dead-stripped; BLACK
# → "LEAF 5" if binding broken.
```

If the 4-check validator passes AND the mode-31 discriminator reports
NON-UNIFORM (LEAF 2), the v24 binding-broken hypothesis is finally closed
empirically and `DIAGNOSTIC_2026-07-30.md` + this drift diagnostic can
both be retired in favor of a single canonical "GBuffer SRV bindings are
live + reading real values in current source" diagnostic.

## Why this is not a v<N> cycle

Per `six-role-pipeline §When NOT to use this skill`:

- All 3 anti-conditions apply (interactive GPU debug; single-profile
  file-only host with terminal blocked by tirith EC-039; surgical-patch-
  adjacent fix already applied).
- Rule 10 fires; planner `[SILENT]`-gate active.
- Authoritative diagnostic (`DIAGNOSTIC_2026-08-29-empirical-closure.md`)
  has now been demonstrated to be stale in part; this drift diagnostic
  is the file-only update.

## Files correlated

| File | Status | What it shows |
|------|--------|---------------|
| `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log` | 273L, 50411 bytes | Authoritative log evidence, byte-equal this turn |
| `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/` | 50 PNGs, ~1 of 8 from the 2026-08-14 22:19 run | **DRIFT**: 7 of 8 PNGs cited by closure diagnostic are gone |
| `docs/DIAGNOSTIC_2026-08-29-empirical-closure.md` | PARTIALLY STALE | mtime-on-artifact evidence contradicts 8-PNG claim |
| `docs/PIPELINE_HEALTH_2026-08-29_six-role-tick-now-507.md` | This turn | Per-tick audit consolidating this drift finding + lineage closure pattern |

— file-only audit, 2026-08-29, autonomous invocation #507 in lineage.
