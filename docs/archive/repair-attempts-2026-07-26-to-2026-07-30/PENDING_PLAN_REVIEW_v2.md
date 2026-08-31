# Pending Plan Review v2

- plan: docs/PENDING_PLAN_v2.md
- verdict: FIX
- reviewer: planner+plan-criticer (single-head autonomous cron)
- timestamp: 2026-07-27T01:05:00Z

## Design soundness
The plan correctly identifies that v1 was falsified by parent verification. The investigation findings (gi_raw R[0,0] G[0,0] B[0,0] from the 00:07 binary that was built from v1 source) are conclusive evidence that v1 did NOT fix the renderer.

However, the plan then makes a critical mistake: it originally proposed adding `CommandList->open()` at line 418 after RenderGBuffer returns. This was WRONG because RenderGBuffer ALREADY opens the CommandList at line 1534 before returning. The "open on already-open" call would trigger nvrhi's `Cannot open a command list that is already open` ERROR (not just a warning), which would drop the post-raster work even more thoroughly.

The plan has been revised to remove the speculative fix and instead recommend diagnostic logging + parent-driven investigation.

## Plan completeness
- Missing files: none (the diagnostic patch is documented but not applied).
- Missing edge cases: the plan should call out that a cron-without-terminal cannot perform interactive debugging. The right approach is to hand back to the parent with concrete diagnostic instructions.

## Feedback for planner (FIX only)
1. **REVERT any speculative patch applied to TestReSTIR_GI_Temporal.cpp.** The original v2 patch (`CommandList->open()` at line 418) was applied but is incorrect — RenderGBuffer already opens the CL at line 1534, so an additional open() would error.
2. **Do NOT apply a one-line fix based on file-only analysis.** The actual root cause (H1, H2, or H3 from the investigation) requires fresh diagnostic data from a live run.
3. **Document the investigation findings and the diagnostic steps needed** so a future cron with terminal access (or the user) can complete the diagnosis.
4. **Mark the v1 KEEP as falsified by parent verification** in PENDING_PICK.md.

The plan needs to be rewritten to remove the incorrect fix and replace it with a "no code change, request parent diagnostic run" approach.

## Revised verdict
FIX — the plan needs the speculative fix removed and replaced with a request for diagnostic data. After my v2 attempt was found to be incorrect (open() on already-open CL), the cron should not apply any code change without parent-driven diagnostic evidence.