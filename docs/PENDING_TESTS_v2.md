# Pending Tests v2 — validate v22-split revert

- commit: docs/PENDING_COMMIT_v2.md
- files: docs/PENDING_TESTS_v2.md (this file — no new test files)
- task: Verify that the v22-split revert in docs/PENDING_COMMIT_v2.md
  fixes the GBuffer SRV binding zero-read bug.

## Test environment requirements

The following are operator-side dependencies (cannot be performed by the
file-only cron). The parent session at the keyboard must execute these.

### Pre-test environment check

```bash
# 1. Vulkan SDK (for slangc — required for shader recompile)
echo "VULKAN_SDK=$VULKAN_SDK"
which slangc
# Expected: $VULKAN_SDK/bin/slangc exists

# 2. ShaderMake binary
ls -la Engine/Binary/GNULinux-x64/ShaderMake/bin/ShaderMake
# Expected: exists

# 3. Python deps for validator
python3 -c "import numpy, PIL; print('numpy/PIL OK')"
# Expected: no ImportError
```

### Test 1: Build with forced shader recompile

```bash
# Clean any stale sblob
rm -f Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.sblob

# Full rebuild + test
./Build.sh --Rebuild --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
```

**Expected:**
- Build exits 0
- `GIPathTracing.sblob` regenerated (mtime newer than before)
- Test executable runs to completion (does not crash)
- No Vulkan VUID-00344 warnings visible in log (validation stubbed)
- Log contains `[v23-diag] binding layout item count=N` where N includes
  14+ items (b0, b1, RTAS, t1, t2, t3, t5, t6, t7, t8, s2, u0, u1, u2,
  plus t9 array = 14 regular + 1 array item = 15 items).

### Test 2: SRV-binding sentinel probe (HLVM_PT_DEBUG_MODE=20)

```bash
./Binary/Debug/.../TestReSTIR_GI_Temporal \
    HLVM_DUMP_RGI=1 \
    HLVM_RGI_ACCUM=8 \
    HLVM_PT_DEBUG_MODE=20 \
    HLVM_RGI_DIAG=1 \
    2>&1 | tee /tmp/rgi_mode20.log

# Inspect gi_raw dump
python3 -c "
from PIL import Image
import numpy as np
img = np.array(Image.open('/path/to/dumps/.../gi_raw_frame8.png'))
print(f'shape={img.shape} dtype={img.dtype}')
print(f'per-channel mean = {img[:,:,0].mean():.2f}, {img[:,:,1].mean():.2f}, {img[:,:,2].mean():.2f}, {img[:,:,3].mean():.2f}')
print(f'per-channel std  = {img[:,:,0].std():.2f}, {img[:,:,1].std():.2f}, {img[:,:,2].std():.2f}, {img[:,:,3].std():.2f}')
print(f'unique values per channel = {len(np.unique(img[:,:,0]))}, {len(np.unique(img[:,:,1]))}, {len(np.unique(img[:,:,2]))}, {len(np.unique(img[:,:,3]))}')
"
```

**Expected:** `per-channel std` > 5/255 (i.e., non-uniform color); NOT
all-zero. Pre-fix: solid black `(0,0,0,255)` with std=0.

### Test 3: WorldPos SRV probe (HLVM_PT_DEBUG_MODE=22)

Same as Test 2 but with `HLVM_PT_DEBUG_MODE=22`. GBufferWorldPos SRV read
should return non-zero. The dump should show Sponza geometry (after
per-channel normalization).

### Test 4: 4-check structural validator

```bash
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py --show
```

**Expected:** Exit code 0, all 4 structural checks PASS
(black% < 5%, color std > 0.04, cell std > 0.04, temporal stability).

### Test 5: Vision review

Open `dumps/.../display_frame8.png`. Should show:
- Recognizable Sponza structure (arches, columns, walls)
- Sane exposure (not pitch-black, not blown-out)
- Visible color variation (not all-gray)
- Reasonable shading (Sponza's columns should cast shadows)

## Acceptance criteria for v2

All five tests above must pass:

1. **Build clean.** No compile errors, no shader-compile errors.
2. **Mode 20 dump non-black.** Per-channel std > 5/255.
3. **Mode 22 dump shows geometry.** Per-channel std > 10/255.
4. **Validator passes.** Exit 0, all checks PASS.
5. **Vision review passes.** Human operator confirms Sponza visible.

If any test fails:
- If Test 1 (build) fails: revert the v22-split revert, file an issue.
  The revert was incorrect; deeper investigation needed.
- If Test 2/3 (SRV dumps) fail with SAME black-frame symptom: the
  binding topology isn't the bug; revert the revert and try
  `setRegisterSpaceAndDescriptorSet(true)` on the binding layout.
- If Test 4 (validator) fails but Tests 2/3 pass: a downstream
  ReSTIR/ReBLUR stage has its own bug, separate from the SRV binding.
- If Test 5 (vision) fails but Test 4 passes: the validator is too
  lenient (per AGENTS.md "4-check structural validator > scalar
  mean-luma gate" but still insufficient for vision).

## Out-of-scope (intentionally not tested)

- ReSTIR Temporal / Spatial / ReBLUR passes (separate code paths,
  separate concerns).
- TestReSTIR_GI_Temporal's bypass mode (the HLVM_RGI_BYPASS env var).
- The cvar-driven exposure tuning.
- Frame-rate performance.

These can be regression-tested separately after the binding fix is
verified.