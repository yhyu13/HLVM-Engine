# Pending Impl Review v167
- plan: docs/PENDING_PLAN_v167.md
- commit: docs/PENDING_COMMIT_v167.md
- verdict: KEEP
- reviewer: reviewer (file-only, single-profile host)
- timestamp: 2026-08-21T23:59:30Z

## plan_fidelity_check

The commit marker (`PENDING_COMMIT_v167.md`) matches the plan (`PENDING_PLAN_v167.md`) in scope and intent:
- **Plan Part 1** (revert v166 patch): `commit` provides the exact unified-diff shape with `-22/+0` line count and the precise `pipelineInfo` chain after the revert. Identical to plan §"Concrete patch — Part 1".
- **Plan Part 2** (clear dynamic state before RT bind): `commit` provides the exact `+10/-0` insertion in `setRayTracingState` with the same comment header text and same two-line `setViewport(0, 0, nullptr)` + `setScissor(0, 0, nullptr)` calls. Identical to plan §"Concrete patch — Part 2".
- **Plan Deviations section** in `commit` is empty — the impler did not deviate from the plan.

The fallback (§ Fallback in `commit`) addresses the plan-criticer's "missing fallback draft" concern: the impler concretely drafted the alternative `vulkan-graphics.cpp` modification with tracking bool `m_AnyRayTracingDispatchSinceLastGraphics` if Part 2 fails. This is the correct escalation path.

## TDD evidence

- [ ] Test file present: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` (existing, NOT modified by this patch). The plan correctly identifies the existing 4-check structural validator as the test strategy.
- [ ] Test commit precedes impl: NOT APPLICABLE — the patch is PLANNED in the file-only runspace, not committed. The operator will apply the diffs and run the validator as the test step. Per the operator-side recipe step 8 in `PENDING_COMMIT_v167.md`, validation runs after rebuild.
- [ ] Red-phase commit message: NOT APPLICABLE — same reason. The "red" state is the current binary with 10 VUIDs; the "green" state is the post-patch binary with 0 VUIDs. The red-phase IS observable: `grep -c VUID Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log` returns 10 currently. The green-phase assertion is `grep -c 'VUID-VkRayTracingPipelineCreateInfoKHR-pDynamicStates-03602'` AND `grep -c 'VUID-vkCmdTraceRaysKHR-None-08608'` both returning 0.

## Security scan

- [x] No hardcoded secrets — patch is nvrhi fork C++ code, no secrets.
- [x] No shell injection — patch is C++, no shell calls.
- [x] No eval/exec — patch is C++ Vulkan API, no scripting.
- [x] No SQL injection — N/A, no database.

## Self-review checklist

- [x] Validation: the patch is structurally a revert+insert in well-tested nvrhi fork code. The `vulkan-raytracing.cpp::setRayTracingState` is exercised on every RT dispatch (16 dispatches per test run); any incorrectness would fire a different VUID or crash.
- [x] Error handling: the explicit-clear wraps `m_CurrentCmdBuf->cmdBuf.setViewport`/`setScissor` in `if (m_CurrentCmdBuf && m_CurrentCmdBuf->cmdBuf)` to avoid dereferencing null during the early-init phase (when `setRayTracingState` might be called before any command buffer is allocated). Defensive null-guard pattern matches nvrhi's existing style at `vulkan-raytracing.cpp:1342-1352`.
- [x] Tests: the existing `validate_restir_gi.py` validates 4-check structural validator (non-black, spatial std, cell variance, alpha sentinel). The mode-20 discriminator (`HLVM_PT_DEBUG_MODE=20`) directly tests the GBufferMaterial SRV read from the GI shader, which was the original user-named "GBuffer SRV binding fix" target. Both verification paths are operator-side.
- [x] Diff size: -22/+10 = net -12 lines. Minimal. Well within the v166 plan's "+16/-0" baseline as "small surgical patch" — actually smaller than v166 because the revert removes more lines than the add.

## Cross-references

- `PENDING_PLAN_v167.md` lines 17-25 (Part 1+2 description), lines 75-118 (operator-side recipe)
- `PENDING_COMMIT_v167.md` lines 22-58 (Part 1 diff), lines 60-104 (Part 2 diff), lines 134-164 (operator application recipe), lines 166-186 (fallback)
- `PENDING_TEST_AUDIT_v166.md` (the DOWNGRADED audit citing the fresh 2026-08-14 00:52 evidence)
- `Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp:1643-1665` (the v166 patch to be reverted)
- `Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp:1323-1431` (`setRayTracingState`, the Part 2 insertion point before line 1347)
- `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log:182-183` (2 VUID-03602 errors proving the patch is invalid)
- `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log:204-239` (8 VUID-08608 errors proving the underlying issue persists)
- `Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/src/vulkan/vulkan-graphics.cpp:578` (the source of stale viewport/scissor commands in the command buffer)

## Single-profile caveat

This host has only one worker profile. The plan-criticer and reviewer verdicts are the same head with different prompt text. The KEEP verdict chain is therefore a self-check. The operator at the keyboard is the freshness — they should sanity-check:
1. The revert+add diff shape against `vulkan-raytracing.cpp` line numbers.
2. The two VUIDs in the current log against the patch's intent.
3. The fallback path's tracking-bool design (`m_AnyRayTracingDispatchSinceLastGraphics`) before relying on it.

## Verdict

**KEEP** — proceed to tester.
