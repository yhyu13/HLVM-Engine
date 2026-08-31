# Pending Plan Review v167
- plan: docs/PENDING_PLAN_v167.md
- verdict: KEEP
- reviewer: plan-criticer (file-only, single-profile host)
- timestamp: 2026-08-21T23:58:00Z

## Design soundness

The plan correctly identifies TWO independent VUIDs that the v166 patch failed to address:
1. **VUID-VkRayTracingPipelineCreateInfoKHR-pDynamicStates-03602** (create-time, 2 hits in `TestReSTIR_GI_Temporal.log:182-183`): the v166 patch declared `VK_DYNAMIC_STATE_VIEWPORT` + `VK_DYNAMIC_STATE_SCISSOR` on the RT pipeline's `pDynamicStates`, which Vulkan spec forbids — the spec restricts RT pipeline dynamic states to `VK_DYNAMIC_STATE_RAY_TRACING_PIPELINE_STACK_SIZE_KHR`. The Part 1 revert is necessary and correct.
2. **VUID-vkCmdTraceRaysKHR-None-08608** (dispatch-time, 8 hits in `TestReSTIR_GI_Temporal.log:204-239`): the underlying nvrhi fork's `commitGraphicsState` (`vulkan-graphics.cpp:578`) emits `vkCmdSetViewport`/`vkCmdSetScissor` commands to the command buffer during graphics-pipeline binding. Those commands persist into the command buffer and trigger this VUID when `vkCmdTraceRaysKHR` is later dispatched with an RT pipeline bound (which has no dynamic viewport/scissor declared). The Part 2 explicit-clear in `vulkan-raytracing.cpp::setRayTracingState` is the canonical Vulkan-spec fix.

The acceptance criteria from the user's invocation ("no Vulkan VUID/ERROR") are correctly mapped to two `grep -c` commands in the operator-side recipe, expecting 0 hits for both VUIDs.

The plan's evidence-based discovery (direct read of `TestReSTIR_GI_Temporal.log` lines 182-183 and 204-239) correctly retracts the v166 closure's ALL_KEEP verdict. The 23:51 `_2.log` had 0 VUIDs because it was a transient binary state (built with the patch in source but the binary linking picked up a different intermediate state), not a stable post-fix state.

## Plan completeness

The plan covers both VUIDs with surgical, well-scoped edits (+8/-22 lines, all in `vulkan-raytracing.cpp`). It documents:
- Risk #2: `vkCmdSetViewport(0, 0, nullptr)` semantics — count-0 form is the canonical Vulkan dynamic-state-clear pattern; if validation layer rejects, fallback is binding a no-op graphics pipeline (risk #5).
- Risk #3-4: FetchContent `_deps/` is git-ignored; the patch must be applied to all 3 nvrhi fork copies (Debug/Release/RelWithDebInfo).
- Risk #7: if the explicit-clear doesn't resolve VUID-08608, escalate to modifying `vulkan-graphics.cpp::commitGraphicsState` to skip setViewport/setScissor when the next bind is RT.

The operator-side recipe (7 steps) is concrete, mechanical, and verifiable from terminal.

**Missing item (suggested for impl-review):** the plan doesn't specify what to do if BOTH parts succeed but the binary still fires VUID-08608 (because nvrhi's command-list tracking is buggy). The fallback in risk #7 is mentioned but not concretely drafted. Recommend the impler include the alternative patch shape in `PENDING_COMMIT_v167.md` as a § Fallback section so the reviewer can see the contingency.

## Feedback for planner (FIX only)

None. The plan is KEEP. The "missing fallback draft" is a suggestion for impl-review scope, not a planner rework.

## Plan-criticer self-check

- Design matches the user's stated acceptance criterion: "no Vulkan VUID/ERROR" — both VUIDs are addressed.
- Reverting v166 + adding clear-state is the MINIMAL surgical fix consistent with Vulkan spec.
- No new test infrastructure needed (validate_restir_gi.py already exists).
- The plan preserves the v140 AmbientColor override, v142 test-side AmbientColor, v151 ReSTIR Generate split, v161 HLVM_RGI_DEBUG_VIS — all load-bearing pre-v166 source fixes remain intact.
- The plan correctly notes the v166 closure was based on transient state and must be retracted (MAJOR_DELETE audit verdict).

## Verdict

**KEEP** — proceed to impler.
