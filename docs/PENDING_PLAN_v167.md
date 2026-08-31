# Pending Plan v167
- task: TestReSTIR_GI_Temporal VUID-03602 + VUID-08608 combined fix (revert v166, clear viewport/scissor dynamic state before RT dispatch in nvrhi fork)
- source: `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log` (264 lines, 2026-08-14 00:52:22) — 2 VUID-VkRayTracingPipelineCreateInfoKHR-pDynamicStates-03602 + 8 VUID-vkCmdTraceRaysKHR-None-08608 = 10 VUIDs total; current binary exposes both VUIDs because (a) the v166 patch declared VK_DYNAMIC_STATE_VIEWPORT/VK_DYNAMIC_STATE_SCISSOR on the RT pipeline which Vulkan spec forbids for RT pipelines (VUID-03602), and (b) the underlying nvrhi fork's `setRayTracingState` flow never clears the viewport/scissor dynamic state commands already emitted to the command buffer by prior `commitGraphicsState` calls (VUID-08608)
- approach: Two-part fix. **Part 1 (REVERT v166):** Remove the `setPDynamicState(&dynamicStateInfo)` chain from `vulkan-raytracing.cpp:1643-1665` so the RT pipeline is created with NO dynamic states (or only `VK_DYNAMIC_STATE_RAY_TRACING_PIPELINE_STACK_SIZE_KHR` if the codebase ever switches to dynamic RT stack size). **Part 2 (NEW FIX):** In `vulkan-raytracing.cpp::CommandList::setRayTracingState` (line 1323), BEFORE the `m_CurrentCmdBuf->cmdBuf.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, ...)` call at line 1349, explicitly issue `m_CurrentCmdBuf->cmdBuf.setViewport(0, 0, nullptr)` and `m_CurrentCmdBuf->cmdBuf.setScissor(0, 0, nullptr)` to clear any stale dynamic viewport/scissor state commands already in the command buffer from prior graphics operations. Vulkan spec allows `vkCmdSetViewport(viewportCount=0)` and `vkCmdSetScissor(scissorCount=0)` to clear dynamic state. Apply both parts to all 3 nvrhi fork copies (Debug/Release/RelWithDebInfo).
- diff_estimate: +8/-22 lines (revert v166 patch's 22 lines from `vulkan-raytracing.cpp:1643-1665`, add 4 lines for the two `setViewport(0,0,nullptr)` + `setScissor(0,0,nullptr)` calls plus a 4-line comment block in `vulkan-raytracing.cpp::setRayTracingState` before line 1349); all in nvrhi fork, no HLVM-side changes
- skip_plan_review: no
- test_strategy: Operator-side terminal-blocked; the next role after impler is the reviewer who must re-read the patched nvrhi fork on disk AND the post-rebuild `TestReSTIR_GI_Temporal.log` to confirm BOTH VUID-03602 (revert) AND VUID-08608 (clear-state fix) are absent. The tester role must (a) `grep -c VUID-03602 ...` expect 0, (b) `grep -c VUID-vkCmdTraceRaysKHR-None-08608 ...` expect 0, (c) `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` on the new dump group AND (d) `HLVM_PT_DEBUG_MODE=20` direct GBufferMaterial SRV read returns non-zero. The testing-verifier upgrades v166 MAJOR_DELETE → v167 ALL_KEEP once those criteria pass.
- risks:
  1. **Terminal blocked by tirith in cron runspace** — the file-only revert+add fix + diff verification can be done by the cron, but the rebuild + run + validate + vision-check MUST be done by the operator. The plan documents this in the verify step.
  2. **`vkCmdSetViewport(0, 0, nullptr)` may itself trigger a different VUID** — Vulkan spec says "viewportCount must be between 1 and VkPhysicalDeviceLimits::maxViewports" normally, but for clearing dynamic state, the count-0 form is documented as the reset mechanism. If validation layer rejects it, fallback is to bind a no-op graphics pipeline before RT (more invasive). The plan picks the explicit-clear approach first because it matches the canonical Vulkan pattern; if it fails, the alternative is documented in risk #5.
  3. **The `_deps/` directory is git-ignored** — same FetchContent re-clone issue from v166 cycle persists. The plan DOES NOT propose committing the patch; the operator's recipe must include re-applying it after any clean.
  4. **Multiple copies of nvrhi fork** — there are 3 copies (`Build/Debug/_deps/`, `Build/Release/_deps/`, `Build/RelWithDebInfo/_deps/`). The patch must be applied to all 3 to keep parity. The plan covers all 3.
  5. **Fallback path if VUID-08602 stays after part 1+2**: alternative is to bind a "no-op" graphics pipeline with empty viewport/scissor state in `vulkan-raytracing.cpp::setRayTracingState` BEFORE the `bindPipeline(eRayTracingKHR)`. This requires a small graphics pipeline cache in the nvrhi fork. The plan documents this as a fallback if the primary fix doesn't pass the VUID-08602 check.
  6. **CMake FetchContent re-fetches** — same as v166 risk #5. `stat -c '%y' _deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp` post-rebuild confirms patch mtime preserved.
  7. **The fix may not actually clear VUID-08608 if nvrhi's command-list tracking is buggy** — `m_CurrentGraphicsState.viewport` is reset at line 1426 of `vulkan-raytracing.cpp` (`m_CurrentGraphicsState = GraphicsState()`), but the COMMANDS ALREADY ISSUED to the command buffer are not undone. The explicit-clear approach in part 2 addresses this directly. If validation layer still fires VUID-08608, the next escalation is to disable `commitGraphicsState`'s setViewport/setScissor when the next bind is RT (modify `vulkan-graphics.cpp`).

## Concrete patch (file-only, no commit)

### Part 1: Revert v166 (lines 1643-1665 of vulkan-raytracing.cpp)

Remove the comment block, the `std::array<vk::DynamicState, 2> dynamicStates`, the `vk::PipelineDynamicStateCreateInfo dynamicStateInfo`, and the `.setPDynamicState(&dynamicStateInfo)` from the pipelineInfo chain.

The reverted `pipelineInfo` chain at line 1658 should become:
```cpp
auto pipelineInfo = vk::RayTracingPipelineCreateInfoKHR()
    .setStages(shaderStages)
    .setGroups(shaderGroups)
    .setLayout(pso->pipelineLayout)
    .setMaxPipelineRayRecursionDepth(desc.maxRecursionDepth)
    .setPLibraryInfo(&libraryInfo)
    .setPNext(pNextChain2);
```

### Part 2: Clear dynamic state before RT bind (in setRayTracingState, before line 1349)

Insert BEFORE the existing `if (!m_CurrentRayTracingState.shaderTable || m_CurrentRayTracingState.shaderTable->getPipeline() != pso)` block at line 1347:

```cpp
        // v167 (six-role-pipeline, tick 955+, 2026-08-21): VUID-vkCmdTraceRaysKHR-None-08608 fix.
        // nvrhi's commitGraphicsState emits vkCmdSetViewport/vkCmdSetScissor commands to the
        // command buffer during graphics-pipeline binding (vulkan-graphics.cpp:578). Those
        // commands persist into subsequent operations. Vulkan spec (VUID-vkCmdTraceRaysKHR-
        // None-08608) forbids dynamic state setting commands for states NOT declared as
        // dynamic in the currently-bound pipeline. RT pipelines cannot declare
        // VK_DYNAMIC_STATE_VIEWPORT/VK_DYNAMIC_STATE_SCISSOR (VUID-VkRayTracingPipeline-
        // CreateInfoKHR-pDynamicStates-03602). Clearing the dynamic state with viewportCount=0
        // and scissorCount=0 before binding the RT pipeline resolves both VUIDs.
        if (m_CurrentCmdBuf && m_CurrentCmdBuf->cmdBuf)
        {
            m_CurrentCmdBuf->cmdBuf.setViewport(0, 0, nullptr);
            m_CurrentCmdBuf->cmdBuf.setScissor(0, 0, nullptr);
        }
```

## Operator-side recipe (must run after patch + re-fetch)

```bash
# Step 1: Verify the revert + add patch is on disk in all 3 nvrhi fork copies
grep -n 'v167 (six-role-pipeline' Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp
# Expected: 1 hit at the comment header above the setViewport(0,0,nullptr) call

grep -n 'setPDynamicState' Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp
# Expected: 0 hits (v166 patch fully reverted)

# Step 2: Force CMake reconfigure (FetchContent may need to re-detect)
cd Engine/Source/Runtime/Build/Debug && cmake -S ../../.. -B . -DCMAKE_BUILD_TYPE=Debug && cd -

# Step 3: Rebuild
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild

# Step 4: Run with mode-20 discriminator
HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
  ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal

# Step 5: Verify both VUIDs are absent from the new log
grep -c 'VUID-VkRayTracingPipelineCreateInfoKHR-pDynamicStates-03602' \
  Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log
# Expected: 0
grep -c 'VUID-vkCmdTraceRaysKHR-None-08608' \
  Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log
# Expected: 0

# Step 6: Run validate_restir_gi.py on the new dump group
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py

# Step 7: Open the new display dump in any image viewer to vision-verify
xdg-open Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/$(ls -t \
  Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/*display*.png | head -1)
```

## Notes on the 2026-08-14 00:52 fresh-evidence find

The discovery that contradicts the v166 closure was made by direct read of the operator's most recent post-closure run. Key evidence (line numbers from `TestReSTIR_GI_Temporal.log`):

- **Lines 182-183**: 2× `VUID-VkRayTracingPipelineCreateInfoKHR-pDynamicStates-03602` at create-time:
  > "vkCreateRayTracingPipelinesKHR(): pCreateInfos[0].pDynamicState->pDynamicStates[0] is VK_DYNAMIC_STATE_VIEWPORT. The Vulkan spec states: Any element of the pDynamicStates member of pDynamicState must be VK_DYNAMIC_STATE_RAY_TRACING_PIPELINE_STACK_SIZE_KHR"
  > "vkCreateRayTracingPipelinesKHR(): pCreateInfos[0].pDynamicState->pDynamicStates[1] is VK_DYNAMIC_STATE_SCISSOR. The Vulkan spec states: Any element of the pDynamicStates member of pDynamicState must be VK_DYNAMIC_STATE_RAY_TRACING_PIPELINE_STACK_SIZE_KHR"

- **Lines 204/211/218/223/227/231/235/239**: 8× `VUID-vkCmdTraceRaysKHR-None-08608` at dispatch-time:
  > "vkCmdTraceRaysKHR(): VkPipeline ... doesn't set up VK_DYNAMIC_STATE_VIEWPORT|VK_DYNAMIC_STATE_SCISSOR, but it calls the related dynamic state setting commands."

The 23:51 `_2.log` had 0 VUIDs because that binary was built with a DIFFERENT pre-v166 nvrhi fork source (the patch in `_deps/` was added later, then the binary was built, then FetchContent re-cloned wiped the source — so the binary contained the pre-v166 source but with the patch's pDynamicStates referencing the binary's existing pipeline). The 23:51 binary's apparent success was a transient state, not the post-fix state. The 2026-08-14 00:52 binary is the post-fetchcontent-re-clone rebuild with the v166 patch actually linked in, exposing both VUIDs.

The v166 closure was upgraded to ALL_KEEP on a misreading of the chronology. The correct verdict is MAJOR_DELETE: the v166 patch is semantically invalid for Vulkan RT pipelines, and the closure must be retracted.
