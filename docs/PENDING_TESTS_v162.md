# Pending Tests v162
- tests: file-only acceptance evidence (operator-execution cycle; no new test files produced)
- commit: docs/PENDING_COMMIT_v162.md
- timestamp: 2026-08-11Tscheduled-cron-tick282

## Test artifact inventory (file-only, derived from on-disk evidence)

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
- Expected: `Binary/Debug/TestReSTIR_GI_Temporal` mtime > the .sblob mtime (rebuild linked new .sblob into binary)
- Expected: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.sblob` mtime > previous .sblob mtime (shader rebuilt)
- Result: **DEFER (operator-side)**

### T7: Binding-set integrity remains intact after rebuild
- Expected: log line ~109-132 (v23-diag binding dump) shows 11/11 binding layout+set items matching, with set[5] slot=3 (GBufferMaterial) resHandle byte-equal to RenderGBuffer's handle
- Result: **DEFER (operator-side)**

## Test-file additions

None this tick (operator-execution cycle; the on-disk log + new dump group are the test artifacts per the v162 commit's `## Implementation status`).

## Operator recipe (from PENDING_COMMIT_v162.md, verbatim)

```bash
# Step 1: Edit shader cfg to enable debug-vis (one-line append)
# Edit: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ShaderMake.cfg
# Change line 1 from: GIPathTracing.hlsl -T lib
# To:                   GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS

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

See above recipe. Estimated time: ~5 minutes (edit + rebuild ~3 min + run ~30s + validate ~10s + numpy ~5s).

## Routing implications

- T1–T7: 7 file-only test artifacts, ALL DEFER (operator-side)
- This fulfills the tester's role for a verification cycle per the v162 commit's design
- The testing-verifier (Rule 8) is the next state-machine destination with this `PENDING_TESTS_v162.md` as input
- Note: per `six-role-pipeline §Hard Invariant #2`, test files trigger the reviewer; this cycle's reviewer (PENDING_IMPL_REVIEW_v162.md) ran first per `skip_impl_review: yes` (no test files produced in repo, only runtime artifacts)