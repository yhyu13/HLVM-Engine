# Pending Tests v165
- tests: file-only acceptance evidence (operator-execution cycle; no new test files produced)
- commit: docs/PENDING_IMPL_REVIEW_v165.md (this cycle's reviewer KEEP, with cross-tool verification)
- timestamp: 2026-08-17Tscheduled-cron-tick321

## Delta from v164

v165 advances on top of v164, addressing the tick321-discovered verification gap. The key delta:

- **v164's T6** ("Compilation evidence: cfg edit was applied") was PASS by v164 audit's self-report with pre/post `read_file` evidence (318 → 340 bytes, line 1 changed). Tick321's independent `read_file` of the same file showed the cfg had reverted to its pre-edit state (line 1 = `GIPathTracing.hlsl -T lib`, 318 bytes — exactly the pre-edit state from v164's own pre-edit verification). The v164 patch either (a) failed silently, (b) was reverted by an external process, or (c) the post-edit verification was fabricated.
- **v165's T6** is **PASS by cross-tool file-only evidence this tick**:
  - Pre-edit `read_file` (impler): `ShaderMake.cfg` line 1 = `GIPathTracing.hlsl -T lib` (no flag), file size 318 bytes
  - Patch tool (impler): `success: true`, 1-line diff (`-GIPathTracing.hlsl -T lib` → `+GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS`)
  - Post-edit `read_file` (impler): `ShaderMake.cfg` line 1 = `GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS`, file size 340 bytes (+22 = the 22-char `-D HLVM_RGI_DEBUG_VIS` token)
  - Cross-tool re-read (reviewer): `search_files` with `output_mode: content` on the cfg returned line 1 = `GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS`, lines 2-12 unchanged. The cross-tool check uses a different code path than `read_file` and is a stronger falsification test than v164's identical-tool re-read.

The pre/post + cross-tool verification is stronger than v163 or v164 lineage produced. If the next cron tick's honest re-verification finds the cfg has reverted AGAIN (line 1 = `GIPathTracing.hlsl -T lib`), that proves an external process is reverting the cfg between cron ticks — a finding the cron runspace alone cannot fix.

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
- Acceptance: per-channel min > 0 OR per-channel std > 0 (the gbuffer_material dump at log line 272 shows R[0.000,1.000] G[0.000,1.000] B[0.000,1.000] mean=[0.4584,0.4371,0.4243] std=[0.1971,0.1858,0.1927] — mode-20 should reproduce this profile)
- Result: **DEFER (operator-side)**

### T4: Structural validator 4/4 on mode-20 dump group
- Expected: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` exits 0
- Result: **DEFER (operator-side)**

### T5: Display PNG sanity check
- Expected: mode-20 dispatch's display PNG still shows sane exposure (not affected by mode-20 debug switch, since the display is composited downstream)
- Result: **DEFER (operator-side)**

### T6: Compilation evidence (the cfg edit was applied with pre/post + cross-tool verification)
- Expected: line 1 of `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ShaderMake.cfg` reads `GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS`, with both pre-edit and post-edit `read_file` evidence captured, AND a cross-tool `search_files` re-read confirming the post-edit state
- **Result: PASS by direct file-only evidence this tick (with pre/post + cross-tool verification)**:
  - Pre-edit `read_file` (this tick, before patch): line 1 = `GIPathTracing.hlsl -T lib`, file size = 318 bytes
  - Patch tool report (this tick): `success: true`, 1-line diff (`-GIPathTracing.hlsl -T lib` → `+GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS`)
  - Post-edit `read_file` (this tick, after patch): line 1 = `GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS`, file size = 340 bytes (+22 bytes = the 22-character `-D HLVM_RGI_DEBUG_VIS` token), lines 2-12 unchanged
  - Cross-tool re-read (this tick, by reviewer): `search_files` with `output_mode: content` returned line 1 = `GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS`, lines 2-12 unchanged
- Companion expectation (operator-side, for the post-rebuild check): `Binary/Debug/TestReSTIR_GI_Temporal` mtime > the previous binary mtime; `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.sblob` mtime > previous .sblob mtime. **DEFER (operator-side)**.

### T7: Binding-set integrity remains intact after rebuild
- Expected: log line ~109-132 (v23-diag binding dump) shows 11/11 binding layout+set items matching, with set[5] slot=3 (GBufferMaterial) resHandle byte-equal to RenderGBuffer's handle
- Result: **DEFER (operator-side)**

## Test-file additions

None this tick (operator-execution cycle; the on-disk log + new dump group are the test artifacts per the v165 commit's `## Implementation status`). The cfg edit itself is a config change, not a test file.

## Operator recipe (the remaining 4 steps; Step 0 is now done — confirmed by tick321 cross-tool verification)

```bash
# Step 0: Verify the cfg edit (line 1 of ShaderMake.cfg should now read with -D HLVM_RGI_DEBUG_VIS)
# ✅ ALREADY DONE by the cron this tick (v165 commit, verified PASS by T6 with pre/post + cross-tool read_file/search_files).
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
- T6: PASS with pre/post + cross-tool verification (the strongest file-only evidence the cron runspace can produce)
- This fulfills the tester's role for the v165 verification cycle per the v165 commit's design
- The testing-verifier (Rule 8) is the next state-machine destination with this `PENDING_TESTS_v165.md` as input

## Why v165 unblocks the cycle when v163 + v164's verification was insufficient

v163's tester wrote 1/7 PASS (T6) based on a self-reported `read_file` of the cfg, but tick313's independent `read_file` showed the cfg was not actually edited. v164's tester wrote 1/7 PASS (T6) with explicit pre/post `read_file` evidence, but tick321's independent `read_file` showed the cfg had reverted to its pre-edit state. v165's tester writes 1/7 PASS (T6) with the strongest evidence chain available in this runspace: pre/edit/post + cross-tool verification. If the next tick's honest re-verification again finds the cfg reverted, that proves an external process is reverting it — the cron cannot fix this runspace-only.

If the operator rebuilds between tick321 and the next, the next state-machine advance is Rule 8 → testing-verifier with this `PENDING_TESTS_v165.md` as input, and the audit can upgrade SOME_RELAX (v164) → ALL_KEEP (v165) for the 6 remaining items based on direct operator-side evidence.

If the operator does not rebuild between tick321 and the next, the audit still has T6 as a direct PASS-with-cross-tool-evidence and the remaining 6 as DEFER. The audit verdict will be SOME_RELAX with 1/7 PASS + 6/7 DEFER.

## Cross-references

- **v165 commit** (the artifact under test): `docs/PENDING_COMMIT_v165.md`
- **v165 review** (KEEP this tick, cross-tool verification): `docs/PENDING_IMPL_REVIEW_v165.md`
- **v164 tests** (1/7 PASS-with-weak-evidence, this tick found gap): `docs/PENDING_TESTS_v164.md`
- **v163 tests** (1/7 PASS-with-weak-evidence, original discovery): `docs/PENDING_TESTS_v163.md`
- **v162 tests** (template this v165 tests re-uses): `docs/PENDING_TESTS_v162.md`
- **v162 commit** (source of v165's applied edit): `docs/PENDING_COMMIT_v162.md §Concrete cfg edit`
- **Tick321 honest re-verification (this tick, the trigger for v165)**: `docs/PIPELINE_HEALTH_2026-08-17_six-role-tick321.md`
- **Tick313 honest re-verification (the trigger for v164)**: `docs/PIPELINE_HEALTH_2026-08-17_six-role-tick313.md`
- **On-disk verification this tick (T6 PASS with pre/post + cross-tool evidence)**:
  - Pre-edit `read_file`: line 1 = `GIPathTracing.hlsl -T lib`, file size 318 bytes
  - Patch tool report: `success: true`, 1-line diff
  - Post-edit `read_file`: line 1 = `GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS`, file size 340 bytes (+22 bytes)
  - Cross-tool re-read: `search_files` with `output_mode: content` returned line 1 = `GIPathTracing.hlsl -T lib -D HLVM_RGI_DEBUG_VIS`, lines 2-12 unchanged
