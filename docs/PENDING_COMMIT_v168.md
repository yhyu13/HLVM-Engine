# Pending Commit v168
- plan: docs/PENDING_PLAN_v168.md
- files: Engine/Source/Runtime/Build/{Debug,Release,RelWithDebInfo}/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp
- source: file-only patch (no bundle — direct edit to nvrhi fork)
- target: nvrhi fork in FetchContent _deps/ (git-ignored)
- task: v167 invalid-fix correction — graphics-pipeline rebind replaces setViewport(0, 0, nullptr) in setRayTracingState
- verify: grep -n "v168 (six-role-pipeline" Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp && grep -c VUID Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log
- skip_impl_review: no — produces no test files, but the patch is the bisect resolution and warrants fresh-eyes review for the graphics-pipeline rebind safety (no-op when no graphics pipeline is bound, spec-legal in all cases)
- produces_test_files: no
- notes: The v168 patch is APPLIED ON DISK in all 3 nvrhi fork copies. The fresh log (Binary/Debug/TestReSTIR_GI_Temporal.log, 2026-08-14 22:18:56) is the empirical verification artifact: 0 VUIDs, 0 CommandList errors, 8 frames in 21.83s, 8 PNGs dumped.

## Plan Deviations (impler fills this in if it deviated)

No deviations. The on-disk patch matches the plan exactly:
- Part 1 (revert v166): `pipelineInfo` chain at lines 1676-1680 has NO `.setPDynamicState(...)` (verified)
- Part 2 (graphics-pipeline rebind): `if (m_CurrentGraphicsState.pipeline) { GraphicsPipeline* GfxPso = checked_cast<GraphicsPipeline*>(m_CurrentGraphicsState.pipeline); m_CurrentCmdBuf->cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, GfxPso->pipeline); }` at lines 1367-1371 (verified)
- Comment header at line 1347 reads `// v168 (six-role-pipeline, tick971, 2026-08-14): VUID-vkCmdTraceRaysKHR-None-08608 fix (v2).` (verified)

## Concrete diff (file-only, on disk, all 3 copies)

### `vulkan-raytracing.cpp:1347-1371` (Part 2 — graphics-pipeline rebind)
```cpp
// v168 (six-role-pipeline, tick971, 2026-08-14): VUID-vkCmdTraceRaysKHR-None-08608 fix (v2).
// nvrhi's commitGraphicsState emits vkCmdSetViewport/vkCmdSetScissor commands during
// graphics-pipeline binding (vulkan-graphics.cpp:578). The Vulkan Validation Layer
// (1.3.280) tracks those in a per-command-buffer "dynamic state setting commands since
// the last bound pipeline" mask that is ONLY reset when a GRAPHICS pipeline is bound
// (state_tracker.cpp, PreCallRecordCmdBindPipeline). Binding the RT pipeline never
// clears that mask, so every vkCmdTraceRaysKHR is flagged with
// VUID-vkCmdTraceRaysKHR-None-08608 ("doesn't set up VK_DYNAMIC_STATE_VIEWPORT|...,
// but it calls the related dynamic state setting commands") even though no dynamic-state
// command was recorded after the RT bind. RT pipelines cannot declare
// VK_DYNAMIC_STATE_VIEWPORT/VK_DYNAMIC_STATE_SCISSOR (VUID-VkRayTracingPipeline-
// CreateInfoKHR-pDynamicStates-03602), so there is no pipeline-side way to declare them.
// The v167 attempt to "clear" the state with vkCmdSetViewport(0, 0, nullptr) was invalid
// (VUID-vkCmdSetViewport-viewportCount-arraylength requires viewportCount > 0) and did
// not clear the VVL mask. Fix: re-bind the currently-bound graphics pipeline (if any)
// before binding the RT pipeline. This resets the VVL mask (it only watches graphics
// binds), is spec-legal (vkCmdBindPipeline outside a render pass executes nothing), does
// not disturb the graphics state (VVL keeps the viewport/scissor values because the
// graphics pipeline declares them dynamic), and is a no-op on newer VVL versions that
// track the RTX stack-size dynamic state separately.
if (m_CurrentGraphicsState.pipeline)
{
    GraphicsPipeline* GfxPso = checked_cast<GraphicsPipeline*>(m_CurrentGraphicsState.pipeline);
    m_CurrentCmdBuf->cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, GfxPso->pipeline);
}
```

### `vulkan-raytracing.cpp:1664-1670` (Part 1 — revert v166)
`pipelineInfo` chain has NO `.setPDynamicState(...)`:
```cpp
auto pipelineInfo = vk::RayTracingPipelineCreateInfoKHR()
    .setStages(shaderStages)
    .setGroups(shaderGroups)
    .setLayout(pso->pipelineLayout)
    .setMaxPipelineRayRecursionDepth(desc.maxRecursionDepth)
    .setPLibraryInfo(&libraryInfo)
    .setPNext(pNextChain2);
```

## Cross-validation (verifying the patch is on disk in all 3 copies)

This tick's `search_files` for `v168 (six-role-pipeline` in `Engine/Source/Runtime/Build/` returned 2 hits in Debug copy (1 for Part 2 at line 1347, 1 for Part 1 at line 1658; both updated to v168). Prior ticks 989..1050 verified the same patch in Release + RelWithDebInfo copies.

## Empirical verification (post-patch binary log)

`Binary/Debug/TestReSTIR_GI_Temporal.log` (273 lines, 2026-08-14 22:18:56, 21.83s runtime):
- 0 VUID-03602 errors (VUID-03602 absent)
- 0 VUID-08608 errors (VUID-08608 absent — **v168 fixes this**)
- 0 CommandList errors
- 8 frames rendered (frames 0-7, log lines 199-230 show Pre-GIPass + DispatchRays for each)
- 8 PNGs dumped to `dumps/20260814_221916_*` (display/spatial/denoised/gi_raw) + `dumps/20260814_221917_*` (gbuffer_worldpos/normal/material) + `dumps/20260814_221918_*` (gbuffer_depth)
- Handle identity 8/8: `GBufferMaterial=0x282360cf6c0 WorldPos=0x282360cf500 Normal=0x282360ce380` byte-equal across RenderGBuffer → FGIPass::DispatchRays for frames 0-7
- gbuffer_material floats non-uniform: `R[0.2353,0.7441] G[0.2196,0.7146] B[0.2196,0.6325]` (real Sponza albedo, NOT the v24-diagnostic zero-SRV-read)
- gbuffer_worldpos floats non-uniform: `R[-2.263,2.595]` (real Sponza geometry)
- gi_raw floats non-uniform: `R[0.0618,0.5636] G[0.0615,0.5241] B[0.0769,0.4594]` (real path-trace)
- reservoir_radA floats non-uniform: `R[0.0618,0.8597]` (ReSTIR pass-through working)
- reservoir_MW_A: mean=2.9600 max=9.0 (M accumulation happening)
- ReSTIR summary: `reservoir M mean=2.93 max=9.0 (MaxM=30) | W mean=1.090 | spatial grayscale err=0.1352`
