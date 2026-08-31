# Pending Plan Review v95

- plan: docs/PENDING_PLAN_v95.md
- verdict: KEEP
- reviewer: plan-criticer (role 2 — same head, single-profile caveat per gpu-rendering-bisect-debug anti-pattern #7)
- timestamp: 2026-07-28T22:15:00Z

## Design soundness
**SHARPER than v93.** Two new file-only probes (P4 alpha-flatten-mask, P5 missing-API-surface) **invalidate the v93 fix recipe as-written** and refine the diagnosis: the v22 split is correct in architecture, but its registration step is missing the API surface. The plan correctly identifies that the v93 "register UAVBindingLayout as second entry" branch requires either (a) adding a new FRayTracingPipeline API method (AddBindingLayout) or (b) collapsing back to single binding set. The plan's risk section correctly notes that (b) reintroduces the nvrhi-deferred-barrier-ordering warning FGIPass was built to fix, making (a) the correct path despite its larger surface area. The plan-criticer concurs.

## Plan completeness
- **One missing item**: the plan should call out that finding (i) — the alpha-flatten at TestReSTIR_GI_Temporal.cpp:1734 — affects the v28 alpha-sentinel verification but does NOT block the diagnostic chain. The parent can determine "did the dispatch body run" via the v3 ENTER/EXIT log at FGIPass.cpp:511/514/631 (which writes to HLVM_LOG and stderr both) instead of relying on a dumped alpha channel. Plan should explicitly cite the v3 ENTER/EXIT log path as the canonical "did the body run" gate.
- **One sharpening**: the dumper-flatten finding (i) is also a separate work item in its own right. The cron should add a follow-up card to PENDING_PICK after the v95 cycle closes, to apply a `DumpRGBA32FTextureAlpha` helper that preserves alpha for v28 verification (or any future alpha-channel-debugging pass). That's scope creep for v95 though — defer.

## Feedback for planner (FIX only)
None — KEEP. Plan is materially correct and sharper than v93. The PENDING_PLAN_v95 is the new authoritative reference; v93/v94 are stale by virtue of the missing-API-surface finding alone.
