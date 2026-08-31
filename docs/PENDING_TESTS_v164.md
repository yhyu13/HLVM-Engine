# Pending Tests v164
- tests: file-only acceptance evidence (operator-execution cycle; no new test files produced)
- commit: docs/PENDING_IMPL_REVIEW_v164.md (this cycle's reviewer KEEP)
- timestamp: 2026-08-17Tscheduled-cron-tick314

## Delta from v163

v164 advances on top of v163, addressing the tick313-discovered verification gap. The key delta:

- **v163's T6** ("Compilation evidence: cfg edit was applied") was PASS by v163 audit's self-report, but tick313's independent `read_file` showed the cfg did NOT have the flag on disk. The v163 patch was either not applied, reverted, or its verification was fabricated.
- **v164's T6** is **PASS by direct file-only evidence this tick with explicit pre/post verification**:
  - Pre-edit `read_file`: `ShaderMake.cfg` line 1 = `GIPathTracing.hlsl -T lib` (no flag)
  - Patch tool: `success: true`, 1-line diff
  - Post-edit `read_file`: `ShaderMake.cfg` line 1 = `GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS`, file size 318 → 340 bytes (+22 = the 22-char `-D HLVM_RGI_DEBUG_VIS` token)

The pre/post verification is the same evidence type v163 lineage claimed but did not preserve (the v163 commit + audit only showed a post-edit snapshot, not a pre-edit baseline + post-edit snapshot pair, leaving room for fabrication or patch-tool failure to be undetected).

This is the only test artifact that flipped from DEFER/PASS-but-unverified to PASS-with-strong-evidence. The remaining 6 tests (T1-T5, T7) are still DEFER (operator-side rebuild + mode-20 run + validator + numpy stats).

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
- Acceptance: per-channel min > 0 OR per-channel std > 0 (the gbuffer_material dump at log line 270 shows R[0.000,0.405] G[0.000,0.270] B[0.000,0.180] mean=[0.46,0.44,0.42] std=[0.20,0.19,0.19] — mode-20 should reproduce this profile)
- Result: **DEFER (operator-side)**

### T4: Structural validator 4/4 on mode-20 dump group
- Expected: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` exits 0
- Result: **DEFER (operator-side)**

### T5: Display PNG sanity check
- Expected: mode-20 dispatch's display PNG still shows sane exposure (not affected by mode-20 debug switch, since the display is composited downstream)
- Result: **DEFER (operator-side)**

### T6: Compilation evidence (the cfg edit was applied with pre/post verification)
- Expected: line 1 of `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ShaderMake.cfg` reads `GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS`, with both pre-edit and post-edit `read_file` evidence captured
- **Result: PASS by direct file-only evidence this tick (with pre/post verification)**:
  - Pre-edit `read_file` (this tick, before patch): line 1 = `GIPathTracing.hlsl -T lib`, file size = 318 bytes
  - Patch tool report (this tick): `success: true`, 1-line diff (`-GIPathTracing.hlsl -T lib` → `+GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS`)
  - Post-edit `read_file` (this tick, after patch): line 1 = `GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS`, file size = 340 bytes (+22 bytes = the 22-character `-D HLVM_RGI_DEBUG_VIS` token), lines 2-12 unchanged
- Companion expectation (operator-side, for the post-rebuild check): `Binary/Debug/TestReSTIR_GI_Temporal` mtime > the previous binary mtime; `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.sblob` mtime > previous .sblob mtime. **DEFER (operator-side)**.

### T7: Binding-set integrity remains intact after rebuild
- Expected: log line ~109-132 (v23-diag binding dump) shows 11/11 binding layout+set items matching, with set[5] slot=3 (GBufferMaterial) resHandle byte-equal to RenderGBuffer's handle
- Result: **DEFER (operator-side)**

## Test-file additions

None this tick (operator-execution cycle; the on-disk log + new dump group are the test artifacts per the v164 commit's `## Implementation status`). The cfg edit itself is a config change, not a test file.

## Operator recipe (the remaining 4 steps; Step 0 is now done)

```bash
# Step 0: Verify the cfg edit (line 1 of ShaderMake.cfg should now read with -D HLVM_RGI_DEBUG_VIS)
# ✅ ALREADY DONE by the cron this tick (v164 commit, verified PASS by T6 with pre/post read_file).
head -1 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ShaderMake.cfg
#   Expected output: GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS

# Step 1: Rebuild
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild

# Step 2: Run with mode 20
HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
  ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal

# Step 3: Validate
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py

# Step 4: Numpy-check mode-20 gi_raw
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
- Step 3: validator exits 0, prints PASS for all 4 checks
- Step 4: per-channel std > 0 (non-uniform), per-channel min > 0 (non-zero material data), non-zero ratio > 0.5 (most pixels have material data)

## Operator follow-up required

Steps 1-4 of the recipe above (Step 0 is done by the cron). Estimated time: ~5 minutes (rebuild ~3 min + run ~30s + validate ~10s + numpy ~5s).

## Routing implications

- T1-T5, T7: 6 file-only test artifacts, ALL DEFER (operator-side)
- T6: PASS with pre/post verification (the strongest file-only evidence the cron runspace can produce)
- This fulfills the tester's role for the v164 verification cycle per the v164 commit's design
- The testing-verifier (Rule 8) is the next state-machine destination with this `PENDING_TESTS_v164.md` as input
- Per `six-role-pipeline §Hard Invariant #2`, test files trigger the reviewer; v164's reviewer (`PENDING_IMPL_REVIEW_v164.md`) ran first per the cycle's design (no test source files produced in repo, only runtime artifacts + the cfg edit)

## Why v164 unblocks the cycle when v163's verification was insufficient

v163's tester wrote 1/7 PASS (T6) based on a self-reported `read_file` of the cfg, but tick313's independent `read_file` showed the cfg was not actually edited. This meant v163's T6 PASS was either (a) fabricated, (b) the patch tool failed silently and v163 didn't re-read the file, or (c) something between v163 and tick313 reverted the edit. In all three cases, the cron did not capture falsifiable pre/post evidence.

v164's tester writes 1/7 PASS (T6) with explicit pre/edit/post evidence: a pre-edit `read_file` snapshot (line 1 lacked the flag, 318 bytes), a `patch` tool report (success), and a post-edit `read_file` snapshot (line 1 has the flag, 340 bytes). This is the same PASS verdict as v163 but with a falsifiable evidence chain — if the cron ever fabricates again, the next tick's `read_file` would catch the discrepancy.

If the operator rebuilds between tick314 and the next, the next state-machine advance is Rule 8 → testing-verifier with this `PENDING_TESTS_v164.md` as input, and the audit can upgrade SOME_RELAX (v163) → ALL_KEEP (v164) for the 6 remaining items based on direct operator-side evidence.

If the operator does not rebuild between tick314 and the next, the audit still has T6 as a direct PASS-with-pre-post-evidence and the remaining 6 as DEFER. The audit verdict will be SOME_RELAX with 1/7 PASS + 6/7 DEFER, same as v163 but with stronger evidence for the 1 PASS.

## Cross-references

- **v164 commit** (the artifact under test): `docs/PENDING_COMMIT_v164.md`
- **v164 review** (KEEP this tick): `docs/PENDING_IMPL_REVIEW_v164.md`
- **v163 tests** (template this v164 tests re-uses, but with pre/post evidence strengthening): `docs/PENDING_TESTS_v163.md`
- **v163 commit** (the prior cycle that the tick313 honest re-verification found did not land): `docs/PENDING_COMMIT_v163.md`
- **v162 recipe** (source of v164's applied edit): `docs/PENDING_COMMIT_v162.md §Concrete cfg edit`
- **Tick313 honest re-verification (the trigger for v164)**: `docs/PIPELINE_HEALTH_2026-08-17_six-role-tick313.md`
- **On-disk verification this tick (T6 PASS with pre/post evidence)**: `read_file` of `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ShaderMake.cfg` (12 lines, line 1 = `GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS`, file size 340 bytes; pre-edit was 318 bytes with line 1 = `GIPathTracing.hlsl -T lib`)
