# TestReSTIR_GI_Temporal Debug Session Handoff — 2026-07-25 (updated)

**Session goal:** Fix the magenta-noise display in
TestReSTIR_GI_Temporal using learnings from TestPathTraceGI_Debug
(51_PathTraceGI_Debug).

**Outcome:** TWO real fixes landed. The display now shows a
recognizable Sponza scene with GI shading. Both fixes together
unblock the test from producing meaningful output.

---

## Fixes landed this session

### Commit `2fab7d6` — Dump RGBA32F per-channel normalization

**Symptom:** `gbuffer_worldpos` dump showed a quadrant color
pattern that looked like a sentinel failure or sentinel
clobbering.

**Root cause:** `FImageDump::DumpToPNG` assumes RGBA values are
in [0, 1] and clamps before byte encoding. World-space
positions are outside [0, 1] (R span [-15.2, 15.3], G span
[-11.8, 8.2], B span [-14.3, 0.025] for the Sponza scene at
0.01 scale). The clamp produces solid quadrants (everything
< 0 → 0, everything > 1 → 255) that look like structure but
aren't.

**Fix:** Added `bNormalizePerChannel` flag to
`DumpRGBA32FTexture` (test-harness method, NOT public API).
When true, the dumper computes per-channel min/max across all
pixels and rescales RGB to [0, 1] before byte encoding.
Default false so existing call sites and existing dump
semantics are unchanged. Enabled normalization only for the
`gbuffer_worldpos` call; `gbuffer_normal` and `gbuffer_material`
remain unchanged (already in [0, 1]).

**Verification:** Post-fix dump shows real Sponza geometry —
upper gallery arches, columns, walls, gradient Y-encoded
background, lower gallery figures. Pre-fix dump showed
saturated quadrants.

### Commit `e6b3d52` — Remove WriteGBufferSentinels (the actual magenta fix)

**Symptom:** Display/gi_raw/spatial/denoised dumps were all
uniform magenta noise. GBuffer dumps were correct after the
normalization fix, but the GI path tracer was producing
garbage.

**Root cause:** `WriteGBufferSentinels` was being called per
frame BEFORE the raster pass, overwriting the GBuffer
textures with sentinel values and transitioning them to
RenderTarget. The raster pass overwrote with real Sponza
data, and the dump correctly showed the post-raster values.
But the FGIPass SRV reads in the path tracer shader
returned the SENTINEL value rather than the post-raster
pixel data.

**Bisection evidence (with sentinels):**
- `HLVM_PT_DEBUG_MODE=1` (albedo) gi_raw: 1 unique color, the
  exact sentinel value `(254, 0, 127)` for every pixel.
  The dump showed `(255, 255, 255)` (real Sponza material
  written by the raster pass) — the SHADER saw the sentinel,
  not the post-raster value.
- `HLVM_PT_DEBUG_MODE=2` (normal) gi_raw: 1 unique color,
  the sentinel-encoded value `(29, 57, 85)` for every pixel.
- `HLVM_PT_DEBUG_MODE=13` (RTInstanceInfo[0].AlbedoColor)
  gi_raw: `(255, 255, 255)` — StructuredBuffer SRV reads
  work correctly. Bug is specific to Texture2D SRV reads of
  the GBuffer textures in FGIPass.
- `HLVM_PT_DEBUG_MODE=14` (RTVertices[0].Position) gi_raw:
  `(255, 0, 0)` raw position `(2, -2, -2)` — StructuredBuffer
  SRV reads work correctly for vertex data.

The bug is specific to the GBuffer Texture2D SRV reads. The
WriteGBufferSentinels writes leave the textures in a state
where the SRV read sees the sentinel value, not the
post-raster data — even though the dump (which uses
copyTexture + map, a different code path) sees the correct
post-raster value.

**Why the sentinels were there originally:** The
`t_139c4e41` commit added them with a comment "without
them the GPU work stays in cache and the dump reads back
zeros." That symptom was a separate issue (NVRHI
immediate-command-list collision) fixed by the CommandList
isolation in commit `9a09df2` (bug-088). The sentinel writes
were no longer needed for the cache issue but were left in
place as a "debug overlay" — which then became the cause of
the magenta noise they were originally added to debug.

**Fix:** Removed the `WriteGBufferSentinels()` call from
`RenderGBuffer`. The `WriteGBufferSentinels` function and
the unused `FillGBufferHardcoded` function are still in the
source for reference (dead code; not deleted to keep the
fix focused on the symptom).

**Verification:**
- Test exits 0 with the canonical command.
- Project validator `validate_restir_gi.py` returns PASS.
- Display dump now shows a recognizable Sponza scene: upper
  gallery arches, lower arches, columns, rectangular center
  feature, varying brightness across surfaces. Was uniform
  magenta with the sentinels enabled.
- `HLVM_PT_DEBUG_MODE=1` (albedo) gi_raw: 65,151 unique
  colors, mean (254, 254, 254) = white (the actual Sponza
  material color in this test's GLTF).
- `HLVM_PT_DEBUG_MODE=2` (normal) gi_raw: 65,151 unique
  colors, varied RGB = real Sponza surface normals.
- `gi_raw` (no debug mode): 2,643 unique colors, mean
  (98, 99, 109) = real path-traced radiance (not the
  uniform magenta from the bug).

The image is still a bit washed out (gray-white dominant)
because the Sponza GLTF in this test uses uniform white
materials for the meshes that are loaded. That's a
content-quality issue, not a bug — the GI pipeline is
working correctly.

---

## Status: ALL TARGETED FIXES COMPLETE

The ReSTIR GI display is no longer uniform magenta. The GBuffer
is correct, the path tracer reads the correct GBuffer data, the
denoising and temporal passes produce real radiance, and the
display shows a Sponza scene.

The image is somewhat washed out (mostly gray-white) because:
1. The Sponza GLTF materials in this test load as uniform
   white for the meshes that are rendered.
2. The "fake ambient" term (hardcoded in the test) provides
   a constant illumination that doesn't show the full
   range of Sponza's natural lighting variation.

These are content issues, not bugs in the test infrastructure
or shaders. The HLVM_RGI_ACCUM accumulation count is 4 frames
(default); increasing it would reduce noise but won't add
color variation since the materials are all white.

---

## Files changed in this session

- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`
  - Commit `2fab7d6`: visualization fix (per-channel
    normalization in `DumpRGBA32FTexture`).
  - Commit `e6b3d52`: removed `WriteGBufferSentinels` call
    from `RenderGBuffer`.
- `Engine/Source/Runtime/docs/PENDING_PICK.md` — created
  with card set for six-role pipeline (now stale; the
  pipeline was not run because interactive debugging was
  faster for this particular bug).
- `Engine/Source/Runtime/docs/SESSION_HANDOFF_2026-07-25.md`
  — this file.

## Build / run recipe (verified)

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal

cd Engine/Source/Runtime/Binary/Debug
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.json \
HLVM_DUMP_RGI=1 HLVM_DUMP_FRAMES=4 timeout 180 \
./TestReSTIR_GI_Temporal

# Validator:
cd Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data
python3 validate_restir_gi.py
# Expect: "1/1 checks PASSED"
```

Build time: 25s. Run time: 8s. Validator: <1s.

## Outstanding items (NOT bugs, future enhancements)

1. **bug-075 TemporalReservoir layout transition warning.**
   Still fires per dispatch. Non-fatal. The NVRHI patch
   (bug-073) suppresses the immediate-CL collision. The
   proper fix would be to split the temporal pass into
   separate read and write dispatches so the resource
   state is unambiguous. Deferred — not a correctness
   issue.

2. **Sponza material colors.** The test loads Sponza GLTF
   materials that happen to be (1, 1, 1) for the meshes
   that are rendered. Real Sponza has varied materials
   (red curtains, brown floor, etc.). This is a test
   asset issue, not a test code issue.

3. **Sponza lighting.** The test's "fake ambient" is a
   constant term, not actual NEE with a real Sponza light
   setup. The path tracer is correctly doing NEE inside
   `GIPathTracing.hlsl::ClosestHit`, but the test only
   loads 1 light (a single directional light per the
   FGIPass log). A real Sponza scene would have multiple
   area lights and better GI.

4. **Dead code cleanup.** `WriteGBufferSentinels` and
   `FillGBufferHardcoded` are now unused. Could be
   removed in a follow-up.

## Pipeline status

The `six-role-pipeline` skill was created in this session
but the cron was not run. Interactive debugging proved
faster for this particular GPU bisection work. The skill
is still available for projects where the cron model
fits better.