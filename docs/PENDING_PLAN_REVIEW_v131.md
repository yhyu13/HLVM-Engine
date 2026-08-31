# Pending Plan Review v131 — three-candidate post-handle-identity diagnostic

- plan: docs/PENDING_PLAN_v131.md
- verdict: KEEP
- reviewer: plan-criticer (this cron tick, role #2)
- timestamp: 2026-07-30 (tick 151)

## Design soundness

The v131 plan addresses the THREE remaining candidate root causes for the GI shader's GBuffer SRV binding issue, after tick 150's empirical evidence falsified the handle-identity hypothesis. The narrowing is principled:

1. **Handle-identity hypothesis (ruled out)**: texture handles differ between RenderGBuffer and FGIPass. Tick 150 logs show all 3 textures match across 3 runs at 3 different handle addresses. Falsified.

2. **Remaining candidates (in scope of v131)**:
   - **Candidate A (slangc dead-strip)**: slangc compiles each entry point independently and may strip SRV reads whose results don't reach the output. Per anti-pattern #7 from `gpu-rendering-bisect-debug`. The `case 31u` probe (with non-trivial arithmetic transformation) discriminates this from binding issues by making the read result observable to slangc's reachability analysis.
   - **Candidate B (image layout transition)**: per `references/nvrhi-deferred-barrier-ordering.md` and AGENTS.md "nvrhi's auto-barrier ordering is fragile". The validation layer is gated off via `DeviceManagerVk4_LifeCycle.cpp` stub, so VUID-00344 isn't catching the issue. The state-query probe discriminates this by exposing the actual texture state at dispatch time.
   - **Candidate C (binding slot mapping)**: per AGENTS.md gotcha "constantBufferOffset defaults to 256". The tick 130 audit confirmed `FBindingLayoutBuilder` zeros offsets in its constructor, but a parent caller could override. The offset-log probe discriminates by exposing the runtime values.

Each candidate is targeted by exactly ONE diagnostic measure (cheap and discriminating). The plan has three parallel cheap probes rather than a single more elaborate one — this matches the seven-step playbook (one variable per experiment).

The plan correctly identifies that:
- The discriminating outcomes map to fix paths (table at the end of v131 plan).
- All three candidates are binding/SRV/RT-pipeline issues.
- Each probe is structurally simple and the patches land file-only this tick.
- Empirical verification requires terminal (parent runspace).

## Plan completeness

The plan covers the three remaining candidates identified in the post-tick-150 analysis. What is NOT in scope of v131 but is correctly identified for follow-up:

- The actual FIX for any confirmed candidate (Step A fix: keep-alive pattern in modes 20/21/22; Step B fix: commitBarriers before setRayTracingState; Step C fix: SetBindingOffsets({0,0,0,0})). These belong in a follow-up plan after the discriminating experiment identifies which (if any) candidate is confirmed.
- Step 4 (slangc-leak test, conditional on A): not run in v131 because A is in scope of v131.
- Step 5 (final fix landing): depends on v131's discriminating outcomes.
- Step 6 (post-fix cleanup): depends on Step 5.

The plan correctly limits its scope to "identify which candidate is the root cause, not fix it." The fix path is one parent-runspace recipe away after discrimination.

The plan correctly references the tick 135 patch-bug fix (`|| debugModeEarly == 30u` in bypassEarlyReturn) — the new case 31u also needs to be added to bypassEarlyReturn's bypass list. **This is an oversight in v131 the impler MUST catch when implementing.** Recommend the impler add `|| debugModeEarly == 31u` to the bypass list in both HLSL copies.

## Feedback for planner (FIX only)

Two minor improvements the impler should apply:

1. **Add `|| debugModeEarly == 31u` to bypassEarlyReturn's bypass list** in BOTH HLSL copies (tick 135 fix pattern). Without this, mode 31 will be masked by the same early-return that masked modes 20/21/22. The bypass list at `GIPathTracing.hlsl:475-476` (Private + Data) must include `31u`.

2. **Verify the nvrhi state-query API name** before landing Candidate B. The plan says `m_pDevice->getTextureState(Desc.GBufferWorldPos)` — but this API may not exist in this nvrhi fork (the fork has been heavily customized). Alternative: log the state just after the `setTextureState` calls return, OR inspect via Vulkan validation (re-enable `createValidationLayer` instead of stubbing it). The impler should grep the nvrhi fork for `getTextureState` / `queryTextureState` / `textureState` to find the correct API. If no API exists, fall back to enabling the validation layer via `DeviceManagerVk4_LifeCycle.cpp`.

3. **Verify `BindingLayout->GetBindingOffsets()` exists**. The nvrhi fork may use a different getter (`GetVulkanBindingOffsets()` or similar). The impler should grep for `BindingOffsets` accessors in the nvrhi fork's binding-layout headers before relying on it.

## Note on honesty floor

This plan is structurally different from v125/v126/v127/v128/v130 (which all targeted handle-identity or variations thereof). v131 targets three distinct remaining candidates based on tick 150's empirical falsification evidence. Spawning v131 is NOT phantom-cycle per HARD INVARIANT #4.

The plan correctly acknowledges that:
- Empirical verification requires terminal (EC-039 blocked in this runspace).
- The probes land file-only; the discriminating experiments close in 60-180 seconds once terminal is available.
- Each probe is structurally simple and gated (no log spam).

## Acceptance gate (inherited from v130, unchanged)

Seven criteria per dispatcher instructions. Cannot be satisfied in file-only runspace; the parent runspace with terminal is required.

## Verdict

**KEEP** with two implementation caveats (add 31u to bypass list, verify nvrhi API names before relying on them).

The plan is correctly designed to discriminate the three remaining candidates with cheap, parallel probes. The structural design follows the seven-step playbook (one variable per experiment, cheap rebuilds, discriminating outcomes that map to fix paths).

The plan is approved for implementation. The impler should apply the two minor improvements above (bypass list inclusion, API verification) during implementation.