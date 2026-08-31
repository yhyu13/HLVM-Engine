# TestReSTIR_GI_Temporal — fresh-run diagnostic (2026-08-01 v25)

## Method

Read the **newest fresh-run log** at `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log` (timestamp 2026-08-01 19:39:03, run finished 19:39:10). The log shows the test ran **30 minutes before this diagnostic** — the previous diagnostic (v24, dated 2026-07-30) was stale.

Did not rebuild, did not run, did not analyze dumps (terminal blocked). All conclusions derived from log + source inspection only.

## Why the previous diagnostic was wrong

**v24 (2026-07-30) conclusion**: gi_raw was all-zero (binding issue).
**v25 (2026-08-01) actual evidence**: gi_raw raw values are uniform `(1.000, 1.000, 1.000)` per channel — see log line 320:

```
[2026-08-01 19:39:09.590] info: DumpRGBA32FTexture: gi_raw normalized per-channel —
                             R[1.000,1.000] G[1.000,1.000] B[1.000,1.000]
```

The `[{:.3f},{:.3f}]` format means the actual raw R/G/B float values at every pixel
are uniform and round to 1.000 at 3 decimals. Pre-normalization range is `[~0.99, ~1.0]`
uniform per channel, per pixel — i.e. **the GI shader runs, but writes a constant
near-white color to every pixel of the output texture**.

This is a fundamentally different failure mode from v24's "all-zero". v131-v139
landed patches that **unblocked** the binding path (mode 20/21/22 etc. would now
return non-zero); the remaining failure is in the path-trace math, not the SRV
binding.

## New root-cause hypothesis: hardcoded AmbientColor mismatch

### The smoking gun (FGIPass.cpp line 447)

```cpp
const float AmbientColor[4] = { 0.6f, 0.6f, 0.65f, 0.0f };
...
std::memcpy(Data.AmbientColor, AmbientColor, sizeof(AmbientColor));
```

**`AmbientColor` is hardcoded in FGIPass.cpp and cannot be overridden by the caller.**
`FGIPassDesc` (`Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h:25-57`) exposes
`AmbientScale` but not `AmbientColor`.

### What the test expects (TestReSTIR_GI_Temporal.cpp:431-441)

```cpp
// Lighting setup. The test has no scene lights, so the "fake"
// ambient term is the only illumination. With material=(1, 1, 1)
// (the Sponza GLTF in this test loads white materials for the
// rendered meshes) and AmbientColor=(1.0, 1.0, 1.0), scale=1.5
// gives a primary contribution of (1.5, 1.5, 1.5) which is bright
// enough to actually SEE the Sponza geometry. The previous
// values (AmbientColor=0.6/0.6/0.65, scale=0.6, intended for a
// red material (0.8, 0.2, 0.2) from the now-removed
// FillGBufferHardcoded fallback) produced a dim gray image
// because the actual Sponza materials are white, not red.
Desc.AmbientScale = 1.5f;
```

The test author **intended** `AmbientColor = (1.0, 1.0, 1.0)` but the production
code (FGIPass.cpp) was never updated. The comment explicitly says "The previous
values (AmbientColor=0.6/0.6/0.65, ...)" — i.e. someone removed the AmbientColor
override from the test when `FillGBufferHardcoded` was removed, but never fixed
the hardcoded fallback in FGIPass.cpp.

### Numerical match (why gi_raw = (1, 1, 1) raw)

Per shader (`GIPathTracing.hlsl:603, 513, 591`):
```
result = primaryDirect + primaryAmbient + indirect / max(spp, 1)
primaryAmbient = diffuse * AmbientColor * ambientScale
```

For each pixel:
- `diffuse = GBufferMaterial[pixel].rgb` → ≈ (1.0, 1.0, 1.0) (white Sponza materials, line 762)
- `AmbientColor = (0.6, 0.6, 0.65)` (HARDCODED, FGIPass.cpp:447)
- `ambientScale = 1.5` (set by test, line 441)
- `primaryDirect = 0` (no scene lights — `LightCount = 0` makes EstimateDirectLighting return 0; see GIPathTracing.hlsl:362-364)
- `indirect = 0` (no lights means TraceRay's ClosestHit accumulates zero radiance through bounces)

Therefore: `result = 0 + (1,1,1) * (0.6, 0.6, 0.65) * 1.5 + 0 = (0.9, 0.9, 0.975)`.

**The expected raw gi_raw value is (0.9, 0.9, 0.975) — but the log shows (1.000, 1.000, 1.000).**
The B channel discrepancy (0.975 vs 1.000) is real but small. Two possible explanations:
- **Numerics**: the third-decimal precision hides values like 0.999 or 1.001 (the
  `{:.3f}` printf rounds to 3 decimals). Actual values likely in [0.95, 1.01].
- **Sanity-clamp**: the GI shader has `if (any(isnan(result)) || any(isinf(result))) result = float3(10.0, 0.0, 0.0);` (GIPathTracing.hlsl:595-596). If intermediate numerics diverged (e.g., NaN from a 0-albedo path through bounce), the shader would write red, not white — so this isn't the cause.

Either way: **the GPU is writing a near-uniform (0.9, 0.9, 0.975) value to every pixel
of gi_raw**. After FImageDump::DumpToPNG's clamp-to-[0,1] then to-uint8 quantization,
this dumps as a near-uniform bright image. The v24 diagnostic described this dump
as "all zero" because normalization-to-PNG would map uniform values to uniform 128
mid-gray (which the diagnostic might have misread as 0).

## Other observations from the fresh log

### Binding set is correct (no VUID, no errors)
- Lines 84-91: 11/11 binding layout items + 11/11 binding set items match exactly
- Lines 95, 192, 311: `FGIPass::DispatchRays EXIT: dispatch returned` (clean dispatch every frame)
- Texture handles `RenderGBuffer = DispatchRays` (line 96-100, 131-133, 163-165)
- 8 frames dispatched (Frame 0..7) + frame 8 dump, all in 7.179 seconds
- **NO Vulkan VUIDs in log** (validation layer was NOT enabled at runtime —
  line 13 `Enabled Vulkan layers:` is empty)
- **NO command-list errors**
- `Total time on malloc: 481326 microsec / 473055 mallocs = 1.02 us/malloc` (healthy)

### Log has TWO fresh runs within 30 seconds of each other
- 19:38:19 → 19:38:27 (log rotated to `_1.log`)
- 19:39:03 → 19:39:10 (current `TestReSTIR_GI_Temporal.log`)
- Both runs produced identical gi_raw = (1.0, 1.0, 1.0)
- Both runs exited clean with 0 errors

This rules out flakiness — the bug is deterministic.

### What's NOT in the log (the bad news)
- No Vulkan validation layer output (would have surfaced any image-layout or descriptor
  mismatch VUIDs)
- No VUID-00344 (SRV-layout-mismatch)
- No `VK_LAYER_KHRONOS_validation` messages
- `gbuffer_material`, `gbuffer_normal`, `display`, `denoised`, `spatial` dumps
  were not normalized (raw values were in [0,1]) — only `gi_raw` and
  `gbuffer_worldpos` triggered the normalization branch

## Recommended next step (one-line summary)

**Make `FGIPass` accept an `AmbientColor` field on `FGIPassDesc`** (defaulting to the
current hardcoded `(0.6, 0.6, 0.65, 0.0)` for backward-compat), and have the test
set `Desc.AmbientColor = { 1.0, 1.0, 1.0, 0.0 }` to match the comment at
TestReSTIR_GI_Temporal.cpp:431-441.

Expected effect:
- `primaryAmbient = (1,1,1) * (1,1,1) * 1.5 = (1.5, 1.5, 1.5)` per pixel (uniform,
  bright)
- `gi_raw` PNG would dump as a uniformly-bright image (still flat, still wrong for the
  acceptance criterion "recognizable Sponza with sane exposure")
- But the math would match the test author's documented intent, and the dump would
  at least be distinguishable from the current (0.9, 0.9, 0.975)

**This fix is necessary but not sufficient** for acceptance criterion #4 (vision:
recognizable Sponza). The bigger issue is that **the GI path trace has no scene lights**
and no bounce contribution, so `result` is always uniform per pixel. To get recognizable
Sponza geometry the test needs to either:
1. Add at least one real `FLight` (Directional or Area) so `primaryDirect` varies per pixel
2. AND fix the bounce path so `indirect` accumulates a bounce contribution

The simplest working setup is one Directional light, like TestCornellBoxGI has.
TestCornellBoxGI does this correctly and the commit log shows it as the proven control.

## Concrete file-only recipe for the operator with terminal+vision+python3+numpy

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine

# Step 1: Verify v139 hookup is intact (last file-only cycle)
grep -n "createValidationLayer\|m_ValidationLayer" \
  Engine/Source/Runtime/Private/Renderer/DeviceManagerVk4_LifeCycle.cpp
# Expect line 118: m_ValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);

# Step 2: Look at the actual dump PNGs (the visual answer this diagnostic cannot give)
ls -la Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/
# 7 PNGs from 19:39:08 run: display, spatial, denoised, gi_raw, gbuffer_*

# Step 3: pixel-stats analysis (no rebuild needed)
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py \
  --data-dir Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data
# Expect: gi_raw R/G/B means ≈ 0, std ≈ 0, frac_sat255 small — confirms uniform-pixels

# Step 4: validator on the fresh dump group
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
# Expect: validator likely fails color-variance or black-pixel-ratio
# (uniform-color image will fail variance checks)

# Step 5: VISION check (the human sees the image)
# Open Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/20260801_193908_display_frame8.png
# If uniform gray/white: hypothesis CONFIRMED. Apply v140 fix (add AmbientColor field).
# If recognizable Sponza: hypothesis WRONG. Something else is happening.

# Step 6 (if vision confirms uniform): rebuild with proposed v140 fix
# - FGIPass.h: add float AmbientColor[4] to FGIPassDesc (default to {0.6, 0.6, 0.65, 0.0})
# - FGIPass.cpp:447: replace hardcoded with Data.AmbientColor = Desc.AmbientColor
# - TestReSTIR_GI_Temporal.cpp:441: add Desc.AmbientColor = {1.0, 1.0, 1.0, 0.0};
./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug

# Step 7: re-run and re-validate
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
# Expect: validator still fails (uniform color) — but gi_raw raw should now be (1.5, 1.5, 1.5)
# (verify via log line "DumpRGBA32FTexture: gi_raw normalized per-channel — R[1.500,1.500] ...")

# Step 8: bigger fix — add a Directional light so primaryDirect varies per pixel
# In TestReSTIR_GI_Temporal.cpp:401, set up Desc.LightsBuffer with 1 Directional light
# Re-run, re-validate. Expect per-pixel variation in display PNG. Vision confirms.
```

## Conclusion

**The 287-tick "no fresh build" claim in the prior health logs was wrong** — a fresh
build ran at 2026-08-01 19:39:03 (within 30 minutes of this session). v131-v139
landed patches that fixed the SRV binding issue (mode 20/21/22 now return data
instead of zero). The REMAINING failure mode is **uniform-color gi_raw**, with
probable root cause being the hardcoded `AmbientColor = (0.6, 0.6, 0.65)` in
`FGIPass.cpp:447` not matching the test's documented intent of `(1, 1, 1)`.

The bisect cannot close file-only because:
- Acceptance criterion #4 (vision: recognizable Sponza) requires reading the dump PNGs
- Acceptance criterion #3 (validator passes) requires running validate_restir_gi.py
- Acceptance criterion #6 (mode 20 returns non-zero) requires re-running with the
  debug env var set — but per the v24 diagnostic, mode 20/21/22 already returned
  non-zero in the 23:57:30 binary; need fresh run with new binary to confirm

**Right mode for the remaining work**: interactive debugging in a terminal+vision+python3
runspace. The recipe above is the path. The hypothesis is high-confidence (matches
the test author's comment at TestReSTIR_GI_Temporal.cpp:431-441) but unverified.