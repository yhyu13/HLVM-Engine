# TestReSTIR_GI_Temporal — interactive diagnostic (2026-07-29)

## Method
Two fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs, log + dumps analyzed
with vision (vision_analyze) + per-pixel numpy stats + grep, not just
scalar validator means. Previous cron's diagnosis was wrong because
it leaned on `validate_restir_gi.py` output without ever looking at the
PNGs.

## What the dumps actually contain (vision-analyzed + numpy)

### `dumps/20260729_235555_gi_raw_frame8.png`
**Mostly black, ~3% bright.** A narrow horizontal strip across the upper
middle where one or two point lights reach some surfaces. The rest is
pitch black. Per-channel log: `R[0.900,96.244] G[0.900,86.793] B[0.975,72.667]`
— HDR radiance IS being produced, just on a tiny fraction of pixels.

### `dumps/20260729_235555_display_frame8.png`
**Recognizable Sponza geometry, washed out and overexposed.**
You can see: main hall arches (top), upper gallery balcony (right side),
columns, ceiling beams, doorway openings (bottom), back wall. BUT the
whole image is at ~80% gray brightness. The back wall has a single
clipped-white rectangle where direct lighting hits a flat surface. The
geometry is recognizable but the image is overexposed.

### `dumps/20260729_235556_gbuffer_material_frame8.png`
**SOLID WHITE at the per-pixel level.** Numpy stats:
- 45.5% of pixels are exactly `(255, 255, 255, 255)`
- 54.5% of pixels are exactly `(254, 254, 254, 255)`
- ZERO per-channel variation outside of the 254/255 boundary
- No magenta, no green, no stone — all RGB channels identical

Decoded from byte → float: every pixel has albedo ≈ `(0.996, 0.996, 0.996, 1.0)`.
This is the **GBuffer rasterizer successfully writing `(~1.0, ~1.0, ~1.0)` to every pixel**, NOT the sentinel value `(0.999, 0.001, 0.500)` from the per-frame sentinel upload (which would decode to `(254, 0, 127)`).

### `dumps/20260729_235556_gbuffer_worldpos_frame8.png`
Real Sponza geometry: back wall (green-to-orange gradient = per-channel
position encoding), upper gallery arches at the top, lower floor arches
at the bottom, painted blue panel on the back wall (a tapestry/decoration).
The worldpos pass works correctly.

### `dumps/20260729_235505_gbuffer_normal_frame8.png`
Real normals — colorful, well-formed, all Sponza surface orientations
visible. The normal pass works correctly.

### `dumps/20260729_235555_denoised_frame8.png`
Same as display but more clipped — denoise + tonemap drives the lit strip
to nearly white.

## Why the renderer is overexposed AND 97% black at the same time

1. **gi_raw is genuinely zero on 97% of pixels** — only a thin strip of
   pixels receive direct light. This is what makes the rest of the image
   render through the "fake ambient" term only:
   `primaryAmbient = diffuse * AmbientColor * AmbientScale` with
   `AmbientColor=(1,1,1)` and `AmbientScale=1.5`, giving `(1.5, 1.5, 1.5)`
   uniform gray.

2. **The 3% lit pixels have HDR radiance up to 96** (per the per-channel
   log) — 96×1.5 = 144 in the final radiance. Tonemap with exposure 1.0
   clips all of that to 1.0 in display → blown-out white rectangle.

3. **Denoise + spatial pass fills the 97% zero region** with neighbor
   information, which then becomes the uniform gray ambient — explaining
   the washed-out "Sponza visible everywhere but flat" look.

## Root cause: every Sponza material albedo is reading as (1, 1, 1)

This is NOT a "lights are too few" problem. With albedo `(1, 1, 1)` for
every surface, NEE direct lighting has nothing to vary by — every surface
returns the same color regardless of which light hit it, and only the
geometry-clipped few pixels reach the lights at all.

Why is every albedo white? Two candidates:

**(A) `MeshMultiMaterialMap` lookup is failing.** Code at lines 1442-1455:
```cpp
auto MatIt = Scene->MeshMultiMaterialMap.find(E2.second);
if (MatIt != Scene->MeshMultiMaterialMap.end() && !MatIt->second.empty())
{
    const auto& M = MatIt->second[0];
    ThisInfo.AlbedoColor[0] = M->AlbedoColor.x;  // ← from per-mesh PBR material
    ...
}
else
{
    ThisInfo.AlbedoColor[0] = 0.7f;  // ← fallback, NOT 1.0!
    ...
}
```
If the map is empty for every mesh, fallback `(0.7, 0.7, 0.7)` would be
written — but the dump shows `(1.0, 1.0, 1.0)`. So either the fallback
branch isn't being taken, OR the fallback value was changed somewhere.

**(B) cbuffer b1 is bound but `AlbedoColor` field reads from padding.**
The C++ `FInstanceInfo` struct has padding[3] (uint) at the end of the
48-byte block. The HLSL `PerInstanceInfo` cbuffer declares:
```hlsl
float3 AlbedoColor;     // 12 bytes, 4-aligned to 16 if at start of cbuffer
uint   AlbedoTextureIndex;
uint   MaterialFlags;
uint3  Padding;
```
If the HLSL cbuffer packs `AlbedoColor` at offset 16 (start of float4
slot) but C++ packs it at offset 12 (after the 4 uint32s preceding it),
the AlbedoColor in the shader reads whatever's at offset 16 — which on
a freshly-allocated buffer is `0` until the writeBuffer happens, then
whatever was at the uninitialized padding region. If the C++ writeBuffer
doesn't actually update the GPU buffer (e.g., the buffer is in
ConstantBuffer state and the writeback requires a transition), the shader
might read `(0,0,0,0)` from the AlbedoColor slot — but `(0,0,0)` would
encode to `(0,0,0)`, not `(1,1,1)`.

**(C) The PS is reading the wrong cbuffer.** If `b1` binding slot in the
binding set doesn't actually map to the shader's `b1` register (or the
binding layout is incorrect), the shader would read whatever constant
buffer was bound to `b1` previously, or a default-initialized buffer.
A default-initialized constant buffer in nvrhi Vulkan backed by
host-visible memory may contain `(1,1,1)` initialization data from the
driver's `vkAllocateMemory` zeroing, which would explain the `(1,1,1)`
output.

The fastest bisect is to **add a debug mode that outputs the actual
`AlbedoColor` field from the HLSL cbuffer**, separate from MRT2:
```hlsl
case 13u: debugColor = float3(AlbedoColor.rgb); break;  // b1 direct read
```
And another that reads GBufferMaterial as a Texture2D SRV:
```hlsl
case 14u: debugColor = GBufferMaterial.Load(int3(pixel, 0)).rgb; break;
```
If case 13 shows `(1,1,1)`, the cbuffer b1 binding is broken. If case
13 shows the per-mesh albedo and case 14 shows `(1,1,1)`, the PS MRT2
write is broken. If both show the correct value, the GI shader's read
of GBufferMaterial via SRV is broken.

## Why the previous cron missed all this
- The cron was blocked by tirith (terminal access denied) and never ran
  the test binary itself. It wrote PENDING_*_v<N>.md markers without
  any fresh evidence.
- The validator (`validate_restir_gi.py`) has 4 checks. Two of them
  PASS on the white material dump: alpha > 254 (alpha is 255 = pass) and
  "best non-black channel mean" (the gbuffer_material pass meets the
  5.00 threshold). Two FAIL on structural stats (display std=0, cell
  std=0). The validator's PASS on material/alpha masked the actual
  failure mode.
- The mean-luma check is structurally unable to detect "uniform white
  where geometry should be visible" — anti-pattern #5 from
  `gpu-rendering-bisect-debug`.

## What's NOT broken (verified)
- GBuffer worldpos: real geometry, real per-channel range.
- GBuffer normals: well-formed, full orientation variety.
- GI dispatch: runs, writes real HDR data on the pixels that have a path
  to lights. No Vulkan VUIDs in this run's log.
- Command list lifecycle: only the soft NVRHI warning "should be
  executed before reopened", not a hard error.
- Image dump encoder (modulo the zero-range → white issue for fully-zero
  textures): produces correct images for textures with non-zero range.

## Recommended fix (next step)
1. Add GBuffer-side debug modes 13/14 to GIPathTracing.hlsl as above.
2. Run with `HLVM_PT_DEBUG_MODE=13` and `=14`. Compare to the current
   gbuffer_material dump. If mode 13 shows the per-mesh albedo colors
   and mode 14 shows uniform white, the PS MRT2 write is broken (cbuffer
   layout mismatch — likely the (B) candidate above).
3. Once cbuffer alignment is verified, fix the MeshMultiMaterialMap
   population so Sponza materials actually appear (stone colors, marble,
   wood, etc.). Currently the map appears to fall through to white for
   all meshes.
4. Lower the default `Exposure = 1.0f` to `0.3f` (or set
   `HLVM_RGI_EXPOSURE=0.3`) so the tonemap doesn't clip the lit strip.
5. Re-validate with `validate_restir_gi.py`. Expect 4/4 PASS plus
   visually correct Sponza.

## Current crons
- `4d9ef7842c63` HLVM ReSTIR six-role autonomous pipeline (5m): PAUSED
- `f76d8941aaad` HLVM ReSTIR goal-loop watchdog (10m): PAUSED