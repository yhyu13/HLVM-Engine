# Pending Plan Review v21

- plan: docs/PENDING_PLAN_v21.md
- verdict: KEEP
- reviewer: cron (single-head; same model as planner; freshness caveat applies per six-role-pipeline anti-pattern #7)
- timestamp: 2026-07-27

## Design soundness

The v21 plan addresses the highest-confidence root-cause cluster identified by the v22 and v23 heartbeats: nvrhi-deferred-barrier-ordering. The evidence chain is direct and well-documented:

1. **Stale log evidence**: `DeviceManager.cpp:52` "A command list should be executed before it is reopened" warning fires 7 times per run (one per frame).
2. **Binding layout evidence**: `FGIPass.cpp:277-291` has both SRV (t1/t2/t3 GBuffer textures → SHADER_READ_ONLY_OPTIMAL) AND UAV (u0 OutputTexture → GENERAL) in the same binding set.
3. **Skill reference evidence**: `gpu-rendering-bisect-debug/references/nvrhi-deferred-barrier-ordering.md` explicitly documents this pattern as the canonical nvrhi bug, with five "fixes that don't work" and the proper fix being binding layout split + two-phase dispatch.
4. **Stale visual evidence**: gi_raw dumps to 0,0,0; the GI dispatch either doesn't run or its UAV write is dropped.

The proposed fix is the canonical nvrhi-recommended solution: split the SRV + UAV bindings into two binding sets, dispatch with both bound. The HLSL register-to-binding mapping is preserved exactly (only the binding-set grouping changes), which means no HLSL recompile is needed and slangc's per-entry-point dead-stripping (the v17/v18/v19 sentinel concern) is unaffected.

The plan correctly:
- Identifies that v21 is a decision-matrix outcome from the v20 evidence (which is parent-driven)
- Stages the v21a binding-layout-split sub-plan as the most likely branch (matching the v22/v23 hypothesis #1)
- Defers execution of v21a until parent confirms the v20 evidence shape matches hypothesis #1
- Enumerates 5 risks with mitigations (nvrhi multi-binding-set support, pipeline recompile, slangc RT shader-table, max binding set count, -Werror cascade, single-head freshness)

## Plan completeness

The plan is comprehensive but has two minor gaps:

1. **The plan does not specify what happens if nvrhi does NOT support multiple binding sets in `rt::State`.** Risk A acknowledges this uncertainty but the fallback ("dispatch SRV-bind in one phase and UAV-bind in a separate dispatch, with a `commitBarriers` between them") requires a different code shape (two `RTPipeline.DispatchRays` calls instead of one with two binding sets). The fallback should be more concretely specified.

2. **The plan's implementation outline uses nvrhi's `BindingLayoutDesc` + `createBindingLayout` for the UAV layout but does not verify that the project's `FBindingLayoutBuilder` is the canonical builder for both layouts.** Looking at the existing code, `FBindingLayoutBuilder` is the project-specific wrapper. Using `BindingLayoutDesc` directly bypasses the wrapper. The plan should use `FBindingLayoutBuilder` for both layouts to maintain consistency.

3. **The plan does not stage v21b..v21i explicitly.** The decision tree lists them in the "Implementation outline" but does not commit to writing them as v22, v23, ... v29 sub-plans. If the v20 evidence points to a different branch, the cron would need to write a new v21b..v21i plan mid-cycle. The plan should state this explicitly: "v21b..v21i will be staged as v22, v23, ... v29 in subsequent cycles based on v20 evidence shape."

These gaps are non-blocking — the plan is sound and the v21a sub-plan is the most likely correct fix. The gaps can be addressed in the v21a impl cycle if v20 evidence confirms the branch.

## Feedback for planner (FIX only)

None at the plan level. The minor gaps (Risk A fallback specifics, FBindingLayoutBuilder consistency, v21b..v21i staging) can be addressed at impl time if v20 evidence confirms the v21a branch. The plan is correctly parent-evidence-gated and does not propose a code change without evidence.

## Verdict rationale

KEEP (not KEEP-with-caveat or FIX) because:

1. The plan correctly identifies the v20 evidence gate and stages v21a as the conditional follow-up.
2. The v21a fix is the canonical nvrhi-deferred-barrier-ordering fix from the project's own skill reference.
3. The plan does NOT apply any source-code change; the v21 cycle is planning only.
4. The plan acknowledges the single-head freshness caveat explicitly (per six-role-pipeline anti-pattern #7).
5. The plan's risk enumeration is honest (5 risks with explicit mitigations).
6. The plan's diff estimate (+60/-30 lines) is realistic for a binding-layout split refactor.

If the v20 evidence confirms the nvrhi-deferred-barrier-ordering hypothesis, the v21a impl cycle will close the loop. If the v20 evidence points elsewhere, v21b..v21i are pre-staged in the decision tree.
