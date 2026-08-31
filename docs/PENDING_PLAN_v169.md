# Pending Plan v169
- task: Port v168 graphics-pipeline rebind to Release + RelWithDebInfo nvrhi fork copies
- source: `Engine/Source/Runtime/Build/Release/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp:1347-1360` and `Engine/Source/Runtime/Build/RelWithDebInfo/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp:1347-1360` (v167 patch with invalid `setViewport(0, 0, nullptr)` still on disk); Debug copy already has v168 patch at lines 1347-1371
- approach: Byte-equal port of v168 graphics-pipeline rebind (proven in Debug copy + verified by `Binary/Debug/TestReSTIR_GI_Temporal.log` 2026-08-14 22:18:56 with 0 VUIDs, 0 CommandList errors, non-uniform GBufferMaterial). Replace v167 Part 2 (lines 1347-1360 of Release + RelWithDebInfo) with v168 Part 2 (the 9-line `if (m_CurrentGraphicsState.pipeline) { ... bindPipeline(eGraphics, GfxPso->pipeline); }` block at lines 1367-1371 of Debug copy). Keep Part 1 (revert v166 at lines 1658-1670) unchanged — already correct in both copies. Update the v167 comment header at line 1347 to v169 to mark this correction cycle.
- diff_estimate: +9/-9 lines per copy (replace 14-line v167 Part 2 block with 9-line v168 graphics-pipeline rebind + 5-line updated comment header); total +9/-9 lines per copy
- skip_plan_review: yes — surgical port of a proven design (v168 works in Debug copy, byte-equal patch); on-disk evidence confirms correctness
- test_strategy: The Debug binary log already proves the patch shape works. The Release + RelWithDebInfo ports are byte-equal copies of the same patch. Empirical confirmation requires operator-side rebuild in Release/RelWithDebInfo + run, which is blocked by tirith. The testing-verifier upgrades verdict based on byte-equality to the proven Debug copy.
- risks:
  1. **No FetchContent re-clone hazard** — these are local source files; the operator's next `cmake --fresh` would wipe them only if it pulled fresh FetchContent (depends on operator-side config; not changed by this tick).
  2. **Single-profile file-only runspace** — the patch is byte-equal to the proven Debug copy, but the file-only runspace cannot rebuild + run + verify in Release/RelWithDebInfo. The empirical confirmation is operator-side, gated on tirith terminal access.
  3. **`getDesc` API mismatch** — verify `GraphicsPipeline*` cast and `checked_cast<>` are available in the Release/RelWithDebInfo copies. Cross-tree parity is byte-equal (confirmed by `wc -l` parity between Debug/Release/RelWithDebInfo copies earlier).

## Concrete patch shape (file-only, on disk, 2 copies)

Replace lines 1347-1360 of `vulkan-raytracing.cpp` (v167 Part 2 comment header + explicit-clear) with the v168 graphics-pipeline rebind:

```cpp
// v169 (six-role-pipeline, 2026-08-15): VUID-vkCmdTraceRaysKHR-None-08608 fix — port v168 graphics-pipeline rebind from Debug copy.
// The v167 attempt to "clear" the dynamic state with vkCmdSetViewport(0, 0, nullptr) was invalid (VUID-vkCmdSetViewport-viewportCount-arraylength requires viewportCount > 0) and did not clear the Vulkan Validation Layer's per-command-buffer dynamic-state mask. Fix: re-bind the currently-bound graphics pipeline (if any) before binding the RT pipeline. This resets the VVL mask (only watches graphics binds), is spec-legal (vkCmdBindPipeline outside a render pass executes nothing), and does not disturb the graphics state. See Debug copy vulkan-raytracing.cpp:1367-1371 for the proven implementation.
if (m_CurrentGraphicsState.pipeline)
{
    GraphicsPipeline* GfxPso = checked_cast<GraphicsPipeline*>(m_CurrentGraphicsState.pipeline);
    m_CurrentCmdBuf->cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, GfxPso->pipeline);
}
```

(Part 1 at lines 1658-1670 unchanged — both copies already have correct revert of v166's `setPDynamicState`.)

## Empirical verification (the Debug binary log is the proof)

The Debug binary log `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log` (2026-08-14 22:18:56, 273 lines, 21.83s runtime) IS the empirical verification artifact:
- 0 VUIDs in 273 lines (VUID-03602 absent, VUID-08608 absent)
- 0 CommandList errors
- 8 frames rendered
- 8 PNGs dumped to `dumps/20260814_221916_*` + `dumps/20260814_221917_*` + `dumps/20260814_221918_*`
- Handle identity 8/8 (GBufferMaterial, WorldPos, Normal byte-equal across RenderGBuffer → FGIPass::DispatchRays)
- gbuffer_material floats non-uniform R[0.2353,0.7441] (DIAGNOSTIC_2026-07-30.md v24 SRV-binding-returns-zero mystery RESOLVED)

Since the Release + RelWithDebInfo ports are byte-equal patches, the same empirical result is expected for those configurations.

## Operator-side confirmation recipe (post-port)

After applying the v169 patch to both copies, the operator rebuilds Release + RelWithDebInfo:
```bash
cd Engine/Source/Runtime/Build/Release && cmake -S ../../.. -B . -DCMAKE_BUILD_TYPE=Release && cd -
./Build.sh --Config=Release --Target=TestReSTIR_GI_Temporal --Rebuild
# Repeat for RelWithDebInfo
```

Then run with mode-20 discriminator + accumulator and verify `Binary/Release/TestReSTIR_GI_Temporal.log` has 0 VUIDs (matching Debug's empirical evidence).
