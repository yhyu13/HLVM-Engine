# Pending Tests v3 — verify v2 + (contingent) verify v3 diagnostic

- commit: docs/PENDING_COMMIT_v3.md (staged, NOT applied; v3 contingent on v2 outcome)
- files: docs/PENDING_TESTS_v3.md (this file — no new test files)
- task: Verify v2 fix works (cheapest measurement). If v2 didn't work,
  apply v3 contingent changes and verify the diagnostic pinpoints the bug.

## Test environment requirements (operator-side)

The following CANNOT be performed by the file-only cron. The parent
session at the keyboard must execute these.

### Pre-test environment check

```bash
echo "VULKAN_SDK=$VULKAN_SDK"
which slangc
ls -la Engine/Binary/GNULinux-x64/ShaderMake/bin/ShaderMake
python3 -c "import numpy, PIL; print('numpy/PIL OK')"
```

### Decision branch

**Test 1a: Verify v2 (DO THIS FIRST — cheapest measurement)**

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
rm -f Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.sblob
./Build.sh --Rebuild --Config=Debug --Target=TestReSTIR_GI_Temporal --Test 2>&1 | tee /tmp/rgi_v2_build.log
```

**Expected:**
- Build exits 0
- `GIPathTracing.sblob` regenerated
- Test executable runs to completion (no crash)
- No Vulkan VUID-00344 warnings (validation stubbed)
- Log contains `[v23-diag] binding layout item count=N` with N=14+ items

**Test 1b: Run with mode 20 (GBuffer SRV sentinel)**

```bash
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 HLVM_RGI_DIAG=1 \
    ./Binary/Debug/.../TestReSTIR_GI_Temporal 2>&1 | tee /tmp/rgi_mode20.log
```

**Expected (v2 worked):**
- `per-channel std` > 5/255 in the gi_raw dump (NOT all-zero)
- `[handle-id]` log lines for the first 4 frames (RenderGBuffer +
  DispatchRays with matching handle addresses)
- Log contains `[v23-diag] binding layout item count=14` (or similar)
- Log contains `FGIPass: per-frame binding set created OK`

**Inspect the dump:**
```bash
python3 -c "
from PIL import Image
import numpy as np
img = np.array(Image.open('<DUMP_DIR>/gi_raw_frame8.png'))
print(f'shape={img.shape} dtype={img.dtype}')
print(f'per-channel mean = {img[:,:,0].mean():.2f}, {img[:,:,1].mean():.2f}, {img[:,:,2].mean():.2f}, {img[:,:,3].mean():.2f}')
print(f'per-channel std  = {img[:,:,0].std():.2f}, {img[:,:,1].std():.2f}, {img[:,:,2].std():.2f}, {img[:,:,3].std():.2f}')
print(f'unique values per channel = {len(np.unique(img[:,:,0]))}, {len(np.unique(img[:,:,1]))}, {len(np.unique(img[:,:,2]))}, {len(np.unique(img[:,:,3]))}')
"
```

**Test 1c: 4-check structural validator**

```bash
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py --show
```

**Expected (v2 worked):** Exit code 0, all 4 structural checks PASS
(black% < 5%, color std > 0.04, cell std > 0.04, temporal stability).

**Test 1d: Vision review**

Open `dumps/.../display_frame8.png`. Should show recognizable Sponza
structure (arches, columns, walls), sane exposure (not pitch-black, not
blown-out), visible color variation (not all-gray), reasonable shading
(Sponza's columns should cast shadows).

**Test 1 verdict:**
- ALL FOUR (1a build + 1b mode 20 + 1c validator + 1d vision) PASS → v2
  fixed the bug. Close the card. Revert any debug-log changes if you
  applied v3 already.
- ANY FAIL → v2 didn't fix it. Proceed to Test 2.

### Test 2: Apply v3 contingent changes (only if v2 didn't work)

If v2 didn't work, apply the staged v3 changes:

```bash
# 1. Extend handle-id logging in FGIPass.cpp
#    Find: if (Desc.FrameIndex < 4u)
#    Replace: if (Desc.FrameIndex < 64u)

# 2. Extend handle-id logging in TestReSTIR_GI_Temporal.cpp
#    Find: if (FrameCount < 4u)  (or similar)
#    Replace: if (FrameCount < 64u)

# 3. Add mode 23 sentinel-compare to BOTH HLSL copies:
#    - Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl
#    - Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl

# 4. Add sentinel upload in TestReSTIR_GI_Temporal.cpp gated by
#    HLVM_PT_DEBUG_MODE=23.

# 5. Rebuild + retest
rm -f Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.sblob
./Build.sh --Rebuild --Config=Debug --Target=TestReSTIR_GI_Temporal --Test 2>&1 | tee /tmp/rgi_v3_build.log
```

### Test 3: v3 diagnostic — handle-identity + sentinel-compare

**Test 3a: Handle-identity (full-frame)**

Run the test again with HLVM_RGI_DIAG=1 and inspect /tmp/rgi_mode20.log:

```bash
grep '\[handle-id\]' /tmp/rgi_mode20.log
```

**Expected:** 8 lines from RenderGBuffer + 8 lines from DispatchRays
(one per frame, since AccumFrames=8).

**Verdict:**
- All handles match (same hex addresses in RenderGBuffer and DispatchRays
  for each frame) → binding layer is wrong at descriptor level. Route
  to v4 fix: investigate Vulkan binding offsets, nvrhi descriptor-write
  path, or setRegisterSpaceAndDescriptorSet(true).
- Any frame shows a mismatch → raster pass recreates textures mid-frame
  and GI has stale handles. Route to v4 fix: capture raster-time
  texture handles and pass them to GI at frame end.

**Test 3b: Sentinel-compare (mode 23)**

```bash
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=23 HLVM_RGI_DIAG=1 \
    ./Binary/Debug/.../TestReSTIR_GI_Temporal 2>&1 | tee /tmp/rgi_mode23.log
```

**Inspect the dump:**
- Alpha channel = 1.0 where the SRV returned the magenta sentinel (handle
  mismatch OR stale descriptor)
- Alpha channel = 0.0 where the SRV returned valid worldpos data

**Expected output:** A dump where most pixels have alpha=0 (correct
binding), with some pixels (where the raster pass wrote the sentinel
and the GI SRV missed it) showing alpha=1.0.

### Acceptance criteria

1. **Test 1 verdict:** v2 worked (all 4 sub-tests pass) → close card.
2. **Test 3 verdict:** handle-identity log + mode 23 dump produce a
   single deterministic answer to "is this handle mismatch or descriptor
   mismatch". The next card (v4) implements the fix.

## Out-of-scope (intentionally not tested)

- ReSTIR Temporal / Spatial / ReBLUR passes (separate code paths).
- TestReSTIR_GI_Temporal's bypass mode (the HLVM_RGI_BYPASS env var).
- The cvar-driven exposure tuning.
- Frame-rate performance.

These can be regression-tested separately after the binding fix is
verified.

## Operator-side dependency acknowledgment

The cron session CANNOT run these tests (terminal blocked by tirith).
The PENDING_TESTS_v3.md is staged so the operator session has the exact
verification recipe ready. This is the file-only mode the six-role-
pipeline skill explicitly describes (per `references/gpu-rendering-
repair.md` and the "File-only mode vs. terminal-enabled mode" section).
