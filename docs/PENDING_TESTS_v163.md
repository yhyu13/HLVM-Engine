# Pending Tests v163
- tests: file-only acceptance evidence (operator-execution cycle; no new test files produced)
- commit: docs/PENDING_IMPL_REVIEW_v163.md (this cycle's reviewer KEEP)
- timestamp: 2026-08-11Tscheduled-cron-tick284

## Delta from v162

v163 advances on top of v162. The key delta:

- **v162's T6** ("Compilation evidence: cfg edit was applied") was DEFER (operator-side). The operator-side application in v162 was a documented recipe; whether the cfg edit landed on disk was pending operator execution.
- **v163's T6** is **PASS by direct file-only evidence this tick**: `read_file` of `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ShaderMake.cfg` confirms line 1 reads `GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS`, lines 2-12 unchanged.

This is the only test artifact that flipped from DEFER to PASS this tick. The remaining 6 tests (T1-T5, T7) are still DEFER (operator-side rebuild + mode-20 run + validator + numpy stats).

## Test artifact inventory (file-only, mixed DEFER + 1 PASS)

For this verification cycle, the "test" is the operator's upcoming rebuild + mode-20 run, expected to land fresh `Binary/Debug/TestReSTIR_GI_Temporal.log` + `dumps/*<new_ts>*_gi_raw_frame*.png` artifacts.

### T1: Test executable launches and completes after rebuild
- Expected log: timestamp on line 1; clean exit on final line
- Result: **DEFER (operator-side)**

### T2: Mode-20 dispatch completes without error
- Expected log: validation layer enabled at line ~14; zero VUID/ERROR/CommandList across all lines
- Result: **DEFER (operator-side)**

### T3: Mode-20 gi_raw PNG contains non-zero GBufferMaterial
- Expected: `dumps/*<new_ts>*_gi_raw_frame*.png` with non-zero, spatially-varying pixel data (mode 20 = `GBufferMaterial.Load(int3(pixel, 0)).rgb`)
- Acceptance: per-channel min > 0 OR per-channel std > 0 (the gbuffer_material dump shows R[0.000,0.405] G[0.000,0.270] B[0.000,0.180] mean=[0.46,0.44,0.42] std=[0.20,0.19,0.19] — mode-20 should reproduce this profile)
- Result: **DEFER (operator-side)**

### T4: Structural validator 4/4 on mode-20 dump group
- Expected: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` exits 0
- Result: **DEFER (operator-side)**

### T5: Display PNG sanity check
- Expected: mode-20 dispatch's display PNG still shows sane exposure (not affected by mode-20 debug switch, since the display is composited downstream)
- Result: **DEFER (operator-side)**

### T6: Compilation evidence (the cfg edit was applied)
- Expected: line 1 of `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ShaderMake.cfg` reads `GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS`
- **Result: PASS by direct file-only evidence this tick** — `read_file` confirms line 1 = `GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS`; lines 2-12 unchanged. The v162 → v163 cfg edit is on disk.
- Companion expectation (operator-side, for the post-rebuild check): `Binary/Debug/TestReSTIR_GI_Temporal` mtime > the previous binary mtime; `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.sblob` mtime > previous .sblob mtime. **DEFER (operator-side)**.

### T7: Binding-set integrity remains intact after rebuild
- Expected: log line ~109-132 (v23-diag binding dump) shows 11/11 binding layout+set items matching, with set[5] slot=3 (GBufferMaterial) resHandle byte-equal to RenderGBuffer's handle
- Result: **DEFER (operator-side)**

## Test-file additions

None this tick (operator-execution cycle; the on-disk log + new dump group are the test artifacts per the v163 commit's `## Implementation status`). The cfg edit itself is a config change, not a test file.

## Operator recipe (the remaining 5 steps; only Step 1 is now done)

```bash
# Step 1: Edit shader cfg to enable debug-vis (one-line append)
# ✅ ALREADY DONE by the cron this tick (v163 commit, verified PASS by T6).
# Edit: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ShaderMake.cfg
# Change line 1 from: GIPathTracing.hlsl -T lib
# To:                   GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS
# Verification: head -1 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ShaderMake.cfg
#   Expected output: GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS

# Step 2: Rebuild
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild

# Step 3: Run with mode 20
HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
  ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal

# Step 4: Validate
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py

# Step 5: Numpy-check mode-20 gi_raw
python3 -c "
import numpy as np
from PIL import Image
import glob
files = sorted(glob.glob('Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/*gi_raw_frame*.png'))
img = np.array(Image.open(files[-1]))
print('Shape:', img.shape)
print('Per-channel mean:', img[..., :3].mean(axis=(0,1)))
print('Per-channel std:', img[..., :3].std(axis=(0,1)))
print('Per-channel min:', img[..., :3].min(axis=(0,1)))
print('Per-channel max:', img[..., :3].max(axis=(0,1)))
print('Non-zero ratio:', (img[..., :3] > 0).any(axis=-1).mean())
"
```

Expected outputs:
- Step 4: validator exits 0, prints PASS for all 4 checks
- Step 5: per-channel std > 0 (non-uniform), per-channel min > 0 (non-zero material data), non-zero ratio > 0.5 (most pixels have material data)

## Operator follow-up required

Steps 2-5 of the recipe above (Step 1 is done by the cron). Estimated time: ~5 minutes (rebuild ~3 min + run ~30s + validate ~10s + numpy ~5s).

## Routing implications

- T1-T5, T7: 6 file-only test artifacts, ALL DEFER (operator-side)
- T6: PASS (1 of 2 sub-assertions; the on-disk cfg edit is verified; the rebuild-linked mtimes are operator-side)
- This fulfills the tester's role for the v163 verification cycle per the v163 commit's design
- The testing-verifier (Rule 8) is the next state-machine destination with this `PENDING_TESTS_v163.md` as input
- Per `six-role-pipeline §Hard Invariant #2`, test files trigger the reviewer; v163's reviewer (`PENDING_IMPL_REVIEW_v163.md`) ran first per the cycle's design (no test source files produced in repo, only runtime artifacts + the cfg edit)

## Why v163 unblocks the cycle when v162 didn't

v162's tester wrote 7 DEFER (0/7 PASS) because none of the acceptance criteria had file-only-evidence support at v162 time. v163's tester writes 1/7 PASS (T6, on-disk cfg edit verified this tick) + 6 DEFER. This advances the audit count: the testing-verifier now has at least one direct file-only evidence item to work with, even before the operator rebuilds.

If the operator rebuilds between tick284 and the next, the next state-machine advance is Rule 8 → testing-verifier with this `PENDING_TESTS_v163.md` as input, and the audit can upgrade SOME_RELAX (v162) → ALL_KEEP (v163) for the 6 remaining items based on direct operator-side evidence.

If the operator does not rebuild between tick284 and the next, the audit still has T6 as a direct PASS and the remaining 6 as DEFER. The audit verdict will be SOME_RELAX with 1/7 PASS + 6/7 DEFER, slightly better than v162's 0/7 PASS.

## Cross-references

- **v163 commit** (the artifact under test): `docs/PENDING_COMMIT_v163.md`
- **v163 review** (KEEP this tick): `docs/PENDING_IMPL_REVIEW_v163.md`
- **v162 tests** (template + recipe this v163 tests re-uses): `docs/PENDING_TESTS_v162.md`
- **v162 recipe** (source of v163's applied edit): `docs/PENDING_COMMIT_v162.md §Concrete cfg edit`
- **On-disk verification this tick (T6 PASS)**: `read_file` of `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ShaderMake.cfg` (12 lines, line 1 = `GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS`, lines 2-12 unchanged from v25/v161/v162 lineage)
