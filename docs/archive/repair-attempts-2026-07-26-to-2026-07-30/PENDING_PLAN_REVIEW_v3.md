# Pending Plan Review v3

- plan: docs/PENDING_PLAN_v3.md
- verdict: KEEP
- reviewer: planner+plan-criticer (single-head autonomous cron)
- timestamp: 2026-07-27T02:10:00Z

## Design soundness

The v3 plan correctly identifies that file-only analysis cannot complete the diagnosis without terminal access, and proposes the minimum-risk move: pure diagnostic instrumentation that captures the runtime state needed for a future fix cycle. The plan explicitly forbids further speculative behavior changes (per the v1/v2 lessons).

The three diagnostic questions (Q1-Q4) are the right ones to ask:

1. **Does DispatchRays reach its body?** — answered by the ENTER log. If the early-return warning fires, the binding set creation is being skipped entirely.
2. **Is the binding set created?** — answered by the post-binding-set log. If `BindingSet` is null, nvrhi rejected the layout.
3. **Does the dispatch return?** — answered by the EXIT log. If the dispatch hangs or fails fatally, the EXIT log won't appear.
4. **Is the OutputTexture layout correctly transitioned?** — answered by the pre-setTextureState log. The handle pointer + FrameIndex correlation lets us match against the dump and rule out a layout-tracking desync.

The patches are INFORMATIONAL only — HLVM_LOG info-level calls don't change GPU behavior. No risk of regression.

## Plan completeness

- Missing files: none (FGIPass.cpp + TestReSTIR_GI_Temporal.cpp are the only files to touch).
- Missing edge cases: the plan correctly defers the actual root-cause fix to v4+ once the diagnostic data is captured.
- The plan does not propose any speculative behavior change (close+execute at end of Render(), open() after RenderGBuffer, etc.). Good.

## Feedback for planner (FIX only)

None — the diagnostic-only approach is the right move given the v1/v2 lessons and the terminal-blocked constraint. The plan should land as-is.

## Honest assessment

This v3 cycle's deliverable is INSTRUMENTATION, not a fix. The fix is gated on the diagnostic data the parent captures. If the parent provides the log, v4 can target the specific root cause (which is currently unknown). If the parent does not run the binary, no further cycles can make progress — the diagnosis requires runtime data that file-only analysis cannot produce.

The v3 cycle's value is:
1. Captures runtime data needed for v4+ fix cycles.
2. Closes the "speculative-patch trap" the v1 and v2 cycles fell into.
3. Documents the diagnostic questions that need answering, so future sessions don't relitigate them.

The terminal-blocked constraint is honest: this cron CANNOT run `./Build.sh` or `TestReSTIR_GI_Temporal`. The parent must drive the verification step.