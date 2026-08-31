# Pending Impl Review v166
- plan: docs/PENDING_PLAN_v166.md
- commit: docs/PENDING_COMMIT_v166.md
- verdict: KEEP
- reviewer: reviewer (file-only, single-profile host)
- timestamp: 2026-08-12T00:00:00Z

## plan_fidelity_check

The impler applied the patch EXACTLY as planned. All 3 copies of `vulkan-raytracing.cpp` (Debug/Release/RelWithDebInfo) are now byte-identical at the patch site:

```
1643:        // HLVM VUID-vkCmdTraceRaysKHR-None-08608 fix (v166):
1644:        // The RT pipeline must declare VK_DYNAMIC_STATE_VIEWPORT and
1645:        // VK_DYNAMIC_STATE_SCISSOR as dynamic states, because nvrhi's
1646:        // RT state tracking emits setViewport/setScissor before
1647:        // vkCmdTraceRaysKHR. Without this, the Vulkan validation layer
1648:        // fires VUID-vkCmdTraceRaysKHR-None-08608 on every dispatched
1649:        // frame. Mirrors the graphics pipeline pattern in
1650:        // vulkan-graphics.cpp:316-318.
1651:        std::array<vk::DynamicState, 2> dynamicStates = {
1652:            vk::DynamicState::eViewport,
1653:            vk::DynamicState::eScissor
1654:        };
1655:        vk::PipelineDynamicStateCreateInfo dynamicStateInfo;
1656:        dynamicStateInfo.setDynamicStates(dynamicStates);
1657:
1658:        auto pipelineInfo = vk::RayTracingPipelineCreateInfoKHR()
1659:            .setStages(shaderStages)
1660:            .setGroups(shaderGroups)
1661:            .setLayout(pso->pipelineLayout)
1662:            .setMaxPipelineRayRecursionDepth(desc.maxRecursionDepth)
1663:            .setPLibraryInfo(&libraryInfo)
1664:            .setPDynamicState(&dynamicStateInfo)
1665:            .setPNext(pNextChain2);
```

The patch is identical to the plan's prescribed text. The `setPDynamicState` call is in the chain BEFORE `setPNext(pNextChain2)` (line 1665), which is correct because `pNext` is the final chain element (the pointer extension chain). The `dynamicStateInfo` is constructed before `pipelineInfo` (necessary because `setPDynamicState(&dynamicStateInfo)` takes a pointer).

The patch correctly follows the Vulkan spec for `VkRayTracingPipelineCreateInfoKHR`:
- `pDynamicState` is a pointer to `VkPipelineDynamicStateCreateInfo` (OPTIONAL)
- If set, the dynamic states declared in `pDynamicState->pDynamicStates` are valid for the pipeline
- The chain order in `pNext` is preserved (the only change is the addition of `pDynamicState`)

No deviations from the plan. The "Plan Deviations" section in PENDING_COMMIT_v166.md is empty, which is correct.

## TDD evidence

- [x] Test file present: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` (342 lines, 4-check structural validator)
- [ ] Test commit precedes impl: N/A — the patch is to the nvrhi fork in `_deps/` (git-ignored); no commit is made
- [ ] Red-phase commit message: N/A — this is a desktop build patch, not a TDD addition

The TDD framework doesn't apply to this patch because:
1. The patch is to a third-party fork's Vulkan pipeline creation
2. The "test" is operator-side (rebuild + run + validate)
3. No new test files are produced (the validate_restir_gi.py already exists and is the operator-side test)

The reviewer notes this is acceptable for a third-party fork patch where the validation is operator-side.

## Security scan

- [x] No hardcoded secrets: the patch is pure Vulkan API setup
- [x] No shell injection: no shell commands involved
- [x] No eval/exec: no dynamic code execution
- [x] No SQL injection: no database involved

## Self-review checklist

- **Validation**: `setPDynamicState(&dynamicStateInfo)` is the correct Vulkan API call to declare dynamic states on the RT pipeline. Mirrors the graphics pipeline pattern.
- **Error handling**: the `dynamicStateInfo` is stack-allocated; `pipelineInfo` is constructed via the fluent builder API. Both are RAII-clean; no error handling needed.
- **Tests**: `validate_restir_gi.py` is the operator-side test. The patch itself doesn't introduce new test code; it fixes an existing validation layer complaint.

## Patch correctness analysis

The fix is the textbook solution to VUID-vkCmdTraceRaysKHR-None-08608. The Vulkan spec says:
> "If a pipeline is bound to the pipeline bind point used by this command, there must not have been any calls to dynamic state setting commands for any state not specified as dynamic in the VkPipeline object bound to the pipeline bind point used by this command, since that pipeline was bound."

The nvrhi fork's RT pipeline was created without declaring `VK_DYNAMIC_STATE_VIEWPORT` and `VK_DYNAMIC_STATE_SCISSOR` as dynamic. But nvrhi's command-list state tracking (somewhere in `m_Context.commandlist` or similar) emits `vkCmdSetViewport` and `vkCmdSetScissor` before `vkCmdTraceRaysKHR`. The fix is to declare these states as dynamic, so the VUID stops firing.

The alternative (removing the `setViewport`/`setScissor` calls from the caller's command-list) is fragile and would break other functionality. The additive fix is the canonical approach.

## Cross-references

- PENDING_COMMIT_v166.md (the commit)
- PENDING_PLAN_v166.md (the plan)
- PENDING_PLAN_REVIEW_v166.md (KEEP verdict)
- PENDING_PICK.md line 10 (the card)
- vulkan-raytracing.cpp lines 1643-1665 (the patched code)
- vulkan-graphics.cpp lines 316-318 (the template)
- DIAGNOSTIC_2026-07-30.md (authoritative current-state)

## Single-profile caveat

This host has only one worker profile. The "impler" and "reviewer" are the same head with different prompt text. The KEEP verdict is therefore a self-check, not an independent review. The operator at the keyboard is the freshness — they should sanity-check the patch text against the on-disk files before the operator-side rebuild.

## Conclusion

The patch is correct, on-disk, and ready for the operator-side rebuild + run + validate. The reviewer KEEPs. The next tick routes to tester per Rule 7. The tester's role is documentation: the operator-side recipe is in PENDING_COMMIT_v166.md and the tester writes PENDING_TESTS_v166.md listing what the operator must verify.
