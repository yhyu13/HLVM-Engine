# Pending Plan Review v166
- plan: docs/PENDING_PLAN_v166.md
- verdict: KEEP
- reviewer: plan-criticer (file-only, single-profile host)
- timestamp: 2026-08-12T00:00:00Z

## Design soundness

The plan correctly identifies the root cause: `vulkan-raytracing.cpp:1643-1649` creates the RT pipeline with no `pDynamicStates`, but Vulkan validation layer flags `VUID-vkCmdTraceRaysKHR-None-08608` because `setViewport`/`setScissor` are issued before `vkCmdTraceRaysKHR` (the caller, nvrhi's RT state tracking). The fix is the textbook additive solution: declare `{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR}` so the dynamic state commands are valid. The fix mirrors the existing graphics pipeline pattern in `vulkan-graphics.cpp:316-318` (the plan cites this — verified by direct read_file). The risk analysis is honest: terminal-blocked rebuild + the FetchContent re-clone risk are real and documented, and the copy-multiplication note (3 copies of nvrhi fork) is correct.

## Plan completeness

The plan has every component the impler needs:
- Exact patch text (verbatim from the graphics pipeline pattern)
- The Vulkan struct chain change (insertion before `auto pipelineInfo = ...`)
- 3 nvrhi fork copies to patch (Debug/Release/RelWithDebInfo)
- Operator-side 7-step recipe
- Re-stated acceptance criteria from PICK card 6
- 5 risks, each with a mitigation or known limitation

The plan does NOT cover:
- Whether the patch should also be applied to other Vulkan validation warnings (e.g., the layout-mismatch warning from the nvrhi auto-barrier ordering gotcha). However, the user's task is specifically VUID-vkCmdTraceRaysKHR-None-08608, so scope is correctly bounded.
- Whether the nvrhi fork patch should be canonicalized by committing to a local fork. This is a meta-question outside the bug fix scope; the plan correctly notes the patch is to `_deps/` which is git-ignored.

## Feedback for planner (FIX only)

None. The plan is correct as written and the patch is the textbook fix. The plan-criticer KEEPs.

## Cross-references

- PENDING_PLAN_v166.md (the plan)
- PENDING_PICK.md line 10 (the card)
- PIPELINE_HEALTH_2026-08-12_planner-tick721.md (planner's audit)
- vulkan-raytracing.cpp lines 1643-1649 (patch site)
- vulkan-graphics.cpp lines 316-318 (the template pattern)
- Tick720 PIPELINE_HEALTH — the discovery that the current log has 8 VUIDs

## Single-profile caveat

This host has only one worker profile. The "planner" and "plan-criticer" are the same head with different prompt text. The KEEP verdict is therefore a self-check, not an independent review. The operator at the keyboard is the freshness — they should sanity-check the plan against the patch text before the impler runs.
