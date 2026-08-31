# Pending Commit v169
- plan: docs/PENDING_PLAN_v169.md
- files: Engine/Source/Runtime/Build/{Release,RelWithDebInfo}/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp
- source: file-only patch (no bundle — direct edit to nvrhi fork copies)
- target: nvrhi fork in FetchContent _deps/ (git-ignored)
- task: Port v168 graphics-pipeline rebind from Debug copy to Release + RelWithDebInfo copies
- verify: search_files for "v169 (six-role-pipeline" in Engine/Source/Runtime/Build/ — expect 2 hits (one per ported copy); search_files for "v167 (six-role-pipeline" in vulkan-raytracing.cpp — expect 0 hits after port; search_files for "VUID-vkCmdTraceRaysKHR" comment header in Release/RelWithDebInfo — expect 2 hits each
- skip_impl_review: no — produces no test files, but the patch is the cross-tree consistency fix and warrants fresh-eyes review for the byte-equality to the proven Debug copy
- produces_test_files: no
- notes: The Debug copy already has the v168 patch and the post-v168 binary log proves it works (0 VUIDs, 8 frames, non-uniform GBufferMaterial). The Release + RelWithDebInfo copies still have the v167 explicit-clear which violates VUID-vkCmdSetViewport-viewportCount-arraylength.

## Plan Deviations (impler fills this in if it deviated)
No deviations. The on-disk patch matches the plan exactly:
- Part 2 (graphics-pipeline rebind) at `vulkan-raytracing.cpp:1367-1371` of Release + RelWithDebInfo copies — byte-equal to Debug copy
- Comment header at line 1347 of both copies reads `// v169 (six-role-pipeline, 2026-08-15): VUID-vkCmdTraceRaysKHR-None-08608 fix — port v168 graphics-pipeline rebind from Debug copy.`
- Part 1 (revert v166) at lines 1658-1670 of both copies unchanged — clean `pipelineInfo` chain with NO `.setPDynamicState(...)`

## Concrete diff (file-only, on disk, 2 copies)

### `vulkan-raytracing.cpp:1347-1371` (Part 2 — graphics-pipeline rebind, ported)
```cpp
// v169 (six-role-pipeline, 2026-08-15): VUID-vkCmdTraceRaysKHR-None-08608 fix — port v168 graphics-pipeline rebind from Debug copy.
// The v167 attempt to "clear" the dynamic state with vkCmdSetViewport(0, 0, nullptr) was invalid (VUID-vkCmdSetViewport-viewportCount-arraylength requires viewportCount > 0) and did not clear the Vulkan Validation Layer's per-command-buffer dynamic-state mask. Fix: re-bind the currently-bound graphics pipeline (if any) before binding the RT pipeline. This resets the VVL mask (only watches graphics binds), is spec-legal (vkCmdBindPipeline outside a render pass executes nothing), and does not disturb the graphics state. See Debug copy vulkan-raytracing.cpp:1367-1371 for the proven implementation.
if (m_CurrentGraphicsState.pipeline)
{
    GraphicsPipeline* GfxPso = checked_cast<GraphicsPipeline*>(m_CurrentGraphicsState.pipeline);
    m_CurrentCmdBuf->cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, GfxPso->pipeline);
}
```

### `vulkan-raytracing.cpp:1658-1670` (Part 1 — revert v166, unchanged)
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

## Cross-validation (verifying the patch is on disk in all 3 copies post-port)
After v169 port:
- Debug copy: `v168 (six-role-pipeline` at line 1347 + graphics-pipeline rebind at lines 1367-1371
- Release copy: `v169 (six-role-pipeline` at line 1347 + graphics-pipeline rebind at lines 1367-1371
- RelWithDebInfo copy: `v169 (six-role-pipeline` at line 1347 + graphics-pipeline rebind at lines 1367-1371
- All 3 copies: `setPDynamicState` count = 0 (Part 1 intact in all)

## Empirical verification (Debug binary log proves patch shape)
`Binary/Debug/TestReSTIR_GI_Temporal.log` (2026-08-14 22:18:56, 273 lines):
- 0 VUID-03602 + 0 VUID-08608 (VUID cascade eliminated by graphics-pipeline rebind)
- 0 CommandList errors
- 8 frames rendered
- 8 PNGs dumped (gbuffer_material R[0.2353,0.7441] non-uniform)
- Handle identity 8/8 (GBufferMaterial, WorldPos, Normal byte-equal)

Same expected result for Release + RelWithDebInfo after v169 port (byte-equal patch).
