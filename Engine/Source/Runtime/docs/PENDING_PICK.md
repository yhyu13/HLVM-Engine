# Six-Role Pipeline — Pending Task Queue (HLVM-Engine rhi2)

This file is the parent-written priority queue for the six-role
pipeline cron running on HLVM-Engine `rhi2` branch. The cron
dispatcher reads this file at every tick and routes to the
appropriate role based on which tasks are `[ ]` (pending) and
which are `[x]` (done).

## Pipeline config

- Workdir: `Engine/Source/Runtime/`
- Branch: `rhi2` (direct commits, no worktree)
- Profile: `claude_coder` (single-profile mode; freshness
  guarantee reduced — see dispatcher prompt caveat)
- Test target: `TestReSTIR_GI_Temporal`
- Build: `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal`
- Verify command (per-frame dump + image inspection):
  ```
  cd Engine/Source/Runtime/Binary/Debug && \
  VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.json \
  HLVM_DUMP_RGI=1 HLVM_DUMP_FRAMES=4 timeout 180 \
  ./TestReSTIR_GI_Temporal
  ```
  Then read the PNGs in
  `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/`
  with vision analysis.

## Status as of 2026-07-25

- [x] **card-0: visualization fix landed** as commit `2fab7d6`
  (`[Test] fix: TestReSTIR_GI_Temporal dump RGBA32F per-channel
  normalization`). gbuffer_worldpos now shows real Sponza
  geometry (arches, columns, walls, gradient background).

- [ ] **card-1: bug-075 (TemporalReservoir layout transition)
  still fires.** Commit 9a09df2 (bug-088) added explicit
  `setTextureState(TemporalReservoir0/1, ShaderResource)` before
  the temporal pass (TestReSTIR_GI_Temporal.cpp:486-489) AND
  before the spatial pass (lines 546-549), but the Vulkan
  validation layer STILL reports:
  ```
  vkCmdDispatch(): Cannot use VkImage [TemporalReservoir0/1] ...
  with specific layout VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
  (specified by sampled image descriptor ... "gHistReservoir0")
  that doesn't match the previous known layout
  VK_IMAGE_LAYOUT_GENERAL.
  ```
  The validation error is non-fatal (NVRHI's bug-073 patch
  suppresses the immediate-CL collision), but the layout
  mismatch is still real and may be causing incorrect SRV reads
  (UB). Investigation needed: does nvrhi's auto-transition
  between the explicit `setTextureState` and the dispatch
  re-promote the texture to GENERAL? Or is the binding set
  creation triggering an implicit GENERAL state somewhere?

- [ ] **card-2: GI magenta noise root cause.** GBuffer is now
  provably correct (per-card-0 fix). The path tracer
  (GIPathTracing.hlsl, 701 lines) produces uniform magenta with
  fine grain instead of Sponza GI. Bug is NOT in the GBuffer
  pipeline. Likely suspects (in priority order):
  1. BLAS/TLAS contents — is the ray-traced BLAS actually
     matching what the raster pass rasterized? If they
     disagree, every GI ray misses or hits garbage.
  2. Light setup — 51 retrospective documented that lights
     coplanar with occluding geometry self-occlude. The
     Sponza light setup in this test may have the same
     issue at the 0.01 scale.
  3. Resource binding for the GI pass — are the three GBuffer
     MRTs actually being read by GIPathTracing.hlsl? Or is
     it reading from a stale/different binding?
  4. Algorithm-level: the "rebuild from ash" ReSTIR GI
     correctness question from claude.md. If everything
     above checks out, the algorithm itself may need the
     rewrite that diagnostic recommends.

- [ ] **card-3 (depends on card-2): if card-2's investigation
  concludes the algorithm is structurally broken, plan the
  rewrite.** Use the 51 retrospective's methodology:
  debug modes 1-N that amputate the chain at each stage,
  constant-sentinel reads on both sides of the TraceRay
  payload boundary, CPU reference render for the scene.

## Methodology notes from 51_PathTraceGI_Debug

The 51 retrospective documented four diagnostic rules that
apply directly here:

1. **Trust measurements, not code reading.** A mean-luminance
   gate ("PASS") hid a broken image for days. Always look at
   the PNGs yourself, never trust the test's scalar verdict.
2. **Bisect with debug modes.** Each pass's debug output
   isolates a stage of the chain. The path tracer here already
   has modes; verify they exist and add new ones if needed.
3. **Constant-sentinel reads.** Compare a known constant
   written in RayGen vs the same constant written in
   ClosestHit. If RayGen reads cleanly and ClosestHit reads
   garbage, the bug is at the payload boundary (slangc dead-
   strip or layout desync).
4. **CPU reference render.** For the path tracer to be
   trustworthy, the underlying scene must be renderable
   without GPU RT. If the CPU reference disagrees with the
   GPU, the scene/camera/light are innocent and the GPU chain
   is guilty.