# TestReSTIR_GI_Temporal Debug Session Handoff — 2026-07-25

**Session goal:** Fix the magenta-noise display in
TestReSTIR_GI_Temporal using learnings from TestPathTraceGI_Debug
(51_PathTraceGI_Debug).

**Outcome:** One concrete fix landed (commit `2fab7d6`). The
remaining magenta symptom requires investigation that benefits
from a longer debugging session or the full six-role pipeline
cron; this handoff captures what's been verified so the next
session can pick up efficiently.

---

## Fixes landed this session

### Commit `2fab7d6` — Dump RGBA32F per-channel normalization

**Symptom:** `gbuffer_worldpos` dump showed a quadrant color
pattern that looked like a sentinel failure or sentinel
clobbering.

**Root cause:** `FImageDump::DumpToPNG` (in
`Private/Image/FImageDump.cpp`) assumes RGBA values are in
[0, 1] and clamps before byte encoding. World-space positions
are outside [0, 1] (R span [-15.2, 15.3], G span [-11.8, 8.2],
B span [-14.3, 0.025] for the Sponza scene at 0.01 scale).
The clamp produces solid quadrants (everything < 0 → 0,
everything > 1 → 255) that look like structure but aren't.

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

**Impact:** bug-074 from `final-state-2026-07-23.md` (the
"dump reads zeros" claim) does NOT exist as described. The
GBuffer raster pass was working correctly; only the dump
visualization was broken.

---

## Bugs still active (handoff to next session)

### bug-075 (real, but non-fatal) — TemporalReservoir layout transition

**Symptom:** Vulkan validation layer reports on every dispatch:

```
[Vulkan] ERROR: vkCmdDispatch(): Cannot use VkImage
[TemporalReservoir0/1] ... with specific layout
VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL (specified by sampled
image descriptor [... "gHistReservoir0/1"]) that doesn't match
the previous known layout VK_IMAGE_LAYOUT_GENERAL.
```

**Code state:** Explicit transitions ARE in place at
`TestReSTIR_GI_Temporal.cpp:486-489` (before temporal dispatch)
and lines 546-549 (before spatial dispatch). The ping-pong
logic at lines 506-510 correctly prevents the same texture from
being bound as both SRV and UAV in the same dispatch.

**Likely cause:** nvrhi's `setTextureState(AllSubresources,
ShaderResource)` is deferred until dispatch time. The binding
set creation inside `DispatchTemporal` (FReSTIRPass.cpp:377)
binds OutReservoir0/1 as UAVs (Texture_UAV at bindings 384/385),
which may promote those textures to GENERAL in nvrhi's
internal layout tracking. When the SRV read at bindings 2/3
(gHistReservoir0/1) then uses a DIFFERENT texture (ping-pong
prevents same-texture collision), the per-resource layout
tracking may still show GENERAL because the ping-pong partner
was just bound as UAV.

**Severity:** Non-fatal. NVRHI's bug-073 patch (validation-
commandlist.cpp `error+return` → `warning` only) suppresses
the immediate-CL collision. The Vulkan dispatch still runs.
Possible UB on the SRV read but probably not the cause of the
magenta symptom (the magenta persists across both buggy and
fixed layouts).

**Suggested fix:** Investigate whether nvrhi's binding-set
layout optimization is doing something unexpected. Possibly
force the transition by calling `commitBarriers()` between
binding-set creation and dispatch. Or split the temporal pass
into two separate dispatches — read-only SRV fetch first,
then write-only UAV update — to keep the resource states
clean. NOT recommended as a single-shot fix; needs deeper
investigation.

### bug-GI — Display magenta noise (the real mystery)

**Symptom:** GBuffer dumps are now correct (per the commit
above), but `display`, `gi_raw`, `spatial`, and `denoised`
dumps are all uniform magenta with fine grain. No Sponza
structure visible.

**Scope:** `GIPathTracing.hlsl` is 701 lines; uses
RayGen + ClosestHit + (likely) AnyHit/Miss entries for the
path tracer. Resources: GBufferWorldPos, GBufferNormal,
GBufferMaterial (the three MRTs, now verified correct),
plus BLAS, plus light setup.

**Suggested investigation order (apply 51 retrospective's
methodology):**

1. **Add debug modes to GIPathTracing.hlsl** mirroring the
   pattern in `Private/Renderer/Shader/GI/GIPathTracing.hlsl`
   and `Test/TestPathTraceGI.cpp`. At minimum:
   - Mode 1: output the worldpos read directly
     (proves the SRV binding reaches the shader)
   - Mode 2: output the normal read directly
     (proves normal SRV binding)
   - Mode 3: output the primary ray hit distance
     (proves TraceRay is hitting anything)
   - Mode 4: output the primary hit worldpos
     (proves the closest hit shader runs and writes payload)
   - Mode 5: output a constant sentinel from ClosestHit
     (proves payload transport works — the 51 decisive
     experiment)
   Add a `HLVM_PT_DEBUG_MODE` CVar or constant buffer field;
   gate each mode on `DebugMode == N`.

2. **Compare against TestCornellBoxGI** — same shader,
   different scene, known to work. If a debug mode shows
   correct data in TestCornellBoxGI but wrong data here,
   the scene setup is the bug. If both are wrong, the
   shader itself is the bug.

3. **CPU reference render** for the Sponza scene at 0.01
   scale, mirroring `TestPathTraceGI`'s
   `RenderCPUReferenceAndDump()`. If the CPU reference
   shows expected radiance distribution, the GPU chain is
   guilty. If it doesn't, the scene itself is the bug
   (camera, light, geometry).

4. **Verify BLAS contents** — does the BLAS the GI pass
   uses match the geometry the raster pass used? The
   0.01 scale comment at GBufferPT_VS.hlsl:84 says "the
   per-instance transform is baked into TLAS at 0.01
   scale." If the raster pass applies 0.01 in ModelMatrix
   but the BLAS doesn't, the two paths disagree on world
   position — the GI ray hits the wrong geometry or misses
   entirely.

### bug-073 (NVRHI validator patch) — status uncertain

The 2026-07-23 patch turned `error(...) + return` into
`warning(...)` (no return) in
`Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/src/validation/validation-commandlist.cpp`.
This bypasses the immediate-command-list collision silently.

**Likely interaction with bug-088:** bug-088's "isolate
CommandList" fix (close+execute+waitForIdle after RenderGBuffer)
prevents the raster pass's work from being retroactively dropped
when a later pass hits a validation error. The NVRHI patch
removes the fatal `error+return` path. Together they explain
why the bug-088 commit message says "with the real
GBufferPT_PS.hlsl restored, the Sponza gbuffer_worldpos dump
shows real geometry."

**Audit verdict:** The patch is necessary given the current
code's pattern of opening multiple immediate command lists
across passes. The cleaner long-term fix would be to ensure
all immediate CL usage goes through a single guard, but
that's a larger refactor. For now, the patch + the
close+execute isolation are working together correctly.

---

## Card set for six-role pipeline (if you want to spin up the cron)

The investigation is structured to fit the 6-role pipeline
(see `PENDING_PICK.md`). Cards in priority order:

- **card-1 (was card-0): bug-075 layout transition.**
  Investigation: does nvrhi's binding-set layout optimization
  actually break the SRV read? Add a sentinel-only debug pass
  that reads TemporalReservoir0/1 as SRV after the temporal
  dispatch and compares against a CPU-side memcpy of the
  resource. If sentinel reads match, the layout error is
  cosmetic (no UB). If they differ, the layout fix is real.

- **card-2 (was card-1): GI magenta root cause.** The big one.
  Plan: add debug modes to GIPathTracing.hlsl (see bullet
  list above), run each mode, bisect the chain to find
  where the data goes wrong. If the algorithm is structurally
  broken (the "rebuild from ash" claim in claude.md), the
  card should conclude with a "rewrite needed" verdict and
  spawn card-3.

- **card-3 (depends on card-2): ReSTIR GI rewrite plan.** Only
  if card-2 concludes the algorithm is unsalvageable. Use
  the 51 retrospective's bisect+sentinel methodology to scope
  what specifically is broken (per the diagnostic in
  claude.md, four issues: no sampling, spatial ignores y,
  alpha mismatch, no RIS). Each issue is a separate card.

---

## File-touch summary this session

- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` —
  visualization fix committed (`2fab7d6`).
- `Engine/Source/Runtime/docs/PENDING_PICK.md` — created with
  card set for six-role pipeline.

## Build / run recipe

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal

cd Engine/Source/Runtime/Binary/Debug
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.json \
HLVM_DUMP_RGI=1 HLVM_DUMP_FRAMES=4 timeout 180 \
./TestReSTIR_GI_Temporal

# Dumps land in:
# Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/
```

Build time after visualization fix: 25s.
Run time to first dump: 8.5s.