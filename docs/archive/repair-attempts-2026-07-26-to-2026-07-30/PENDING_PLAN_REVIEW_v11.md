# Pending Plan Review v11
- plan: docs/PENDING_PLAN_v11.md
- verdict: KEEP
- reviewer: plan-criticer (single-head autonomous cron — software-development-practices §"Full auto" anti-pattern #7 caveat applies)
- timestamp: 2026-07-27T09:00:00Z (estimated cron tick wall clock)

## Design soundness

The v11 plan correctly identifies v10's source/binary mismatch conclusion as the dominant remaining hypothesis and applies a minimal, dormant patch that gives the parent a guaranteed-bypass diagnostic surface for the next rebuild. The patch is well-scoped: 2 files, 25 lines added, 0 lines of behavior change when `HLVM_FORCE_CERR_LOGGING` is undefined. The macro gating pattern is the right one for "diagnostic surface that should not run in production" — explicit opt-in, dormant by default, no runtime cost when off. The patch placement is correct: cerr writes go BEFORE the v3 early-return guard at FGIPass.cpp:458-462 (so they fire even if the dispatch body returns early) and BEFORE the NvrhiDevice/Framebuffer early-return at TestReSTIR_GI_Temporal.cpp:378-379 (so they fire even on a null-device run, though that's a degenerate case).

The decision matrix for v11 forward is well-grounded in actual v9 evidence (gi_raw=0, command-list warning every frame, v3 spdlog markers missing from log) and the patches are correctly staged for each outcome. Single-head self-check caveat applies per the dispatcher prompt.

## Plan completeness

The plan covers:
- ✅ Source files modified (2) with line counts
- ✅ Macro gating pattern documented
- ✅ Default-dormant behavior explicitly stated
- ✅ Three-explanation decision matrix (v6a-2, v6a-d, source/binary mismatch)
- ✅ All four forward paths from v11 to v11b/c/d/e
- ✅ Honest structural-limitation path (parent cannot rebuild)
- ✅ `validate_restir_gi.py` continues to apply unchanged
- ✅ Build commands spelled out (default + macro-defined)

What v11 does NOT do (explicitly out of scope, per the plan's "v6a-2/v6a-d" still-open branch):
- ❌ Apply the auto-barrier patch (v6a-2)
- ❌ Apply the binding-layout split (v6a-d)
- ❌ Try to rebuild the binary (terminal blocked)
- ❌ Re-execute the validator (terminal blocked)

These are correctly deferred to v11b/c/d/e, gated on the next parent rebuild's evidence shape.

## Risks acknowledged

The plan correctly identifies that:
- The patch costs 2 new `#include <iostream>` lines (small but non-zero build-time cost)
- The patch adds ~600 bytes of binary size from iostream template machinery (negligible)
- If the patch is never activated by parent rebuild, it has no observable effect (pure file-only cycle)
- The cerr writes do not bypass Vulkan validation; they only bypass spdlog

No risk is understated. The "lowest possible when macro undefined" risk rating is correct.

## Feedback for planner (FIX only)

None — plan is sound and ready for impl.

## Verdict

KEEP. v11 plan is ready to commit. Proceed to impl.
