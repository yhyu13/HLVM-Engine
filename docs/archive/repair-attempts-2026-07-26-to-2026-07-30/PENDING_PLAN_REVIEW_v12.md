# Pending Plan Review v12
- plan: docs/PENDING_PLAN_v12.md
- verdict: KEEP
- reviewer: plan-criticer (single-head autonomous cron — software-development-practices §"Full auto" anti-pattern #7 caveat applies)
- timestamp: 2026-07-27T13:00:00Z (estimated cron tick wall clock)

## Design soundness

v12 is a well-grounded diagnostic escalation. The plan correctly identifies that v11's macro-gated cerr-patch is the wrong default for the current situation: the parent's log evidence (gi_raw=0, command-list warning every frame, zero v3 spdlog markers) confirms the v9 hypothesis set (source/binary mismatch vs spdlog-level-filter) is the right question, and v11's dormant default gives parent no signal to distinguish them. v12 un-gates the cerr writes so the next parent rebuild produces visible diagnostic in stderr regardless of spdlog configuration. This is the maximally-informative file-only move.

The decision matrix (v12a vs v12e vs pipeline-complete) is concrete and keyed to the post-rebuild evidence shape. The patch is minimal (-4 lines total across 2 files) and fully reversible (adding the `#ifdef`/`#endif` back restores v11 byte-for-byte).

The patch placement is correct: BEFORE the early-return guards, so the cerr writes fire even on the earliest possible return. The cerr content is informative (bIsInitialized, RTPipeline.Initialized, SceneTLAS ptr, OutputTex ptr, Frame) — enough to distinguish "function not reached" from "function reached with bad inputs" from "function reached with good inputs but downstream failed."

The plan correctly identifies the v11 `<iostream>` includes as now-load-bearing (no longer conditional) — std::cerr is a standard library symbol always available via `<iostream>`, so the includes work.

## Plan completeness

One minor gap: the plan does not address what happens if parent does a default rebuild (no `-DHLVM_FORCE_CERR_LOGGING`) and the cerr lines still don't appear in stderr. This would be a "cerr is not reaching stderr" outcome (e.g., stderr redirected by the test harness, buffered output not flushed before exit, or the build silently dropped the v12 patch). The plan's decision matrix v12c covers this case as "cerr does NOT fire" but the description could be sharper — specifically, the diagnosis is "investigate stderr buffering or output capture." Acceptable as-is; the parent will report the actual evidence shape and the next cycle can investigate.

Another minor gap: the plan does not explicitly say what to do if BOTH cerr and v3 spdlog markers fire AND gi_raw becomes non-zero AND display is correct. The plan's decision matrix covers this case ("pipeline complete v6d") but does not explicitly call for the follow-up cycle to REVERT the cerr writes. The follow-up is implied in the v6d branch ("revert cerr writes in a follow-up cycle") but the marker files should make this explicit. Adding a note to the v12a/e decision matrix that v6d also requires a v12-revert cycle is a reasonable future addition but not blocking for v12.

## Feedback for planner (FIX only)

None — the plan is sound. KEEP the patch as designed. The two minor gaps above are not blocking; they are reasonable defaults that can be addressed in follow-up cycles if the evidence shape requires.

## Decision

KEEP. Proceed to impler. The patch is:
- ✅ Minimal (-4 lines total)
- ✅ Reversible (add `#ifdef`/`#endif` back to restore v11)
- ✅ Maximally informative (every parent rebuild will produce cerr output)
- ✅ Distinguishes H-A (source/binary mismatch) from H-B (spdlog-level-filter)
- ✅ Preserves v3 spdlog markers (no change to those)
- ✅ Preserves v5 HLVM-bypass removal (no change to that)
- ✅ Preserves bug-088 fix at line 675
- ✅ Preserves bug-075 binding-layout split
- ✅ No test files modified
- ✅ No validator changes

## Single-head caveat

Per software-development-practices §"Full auto" anti-pattern #7, the plan-criticer and planner are the same head. The KEEP verdict is a self-check. Parent's run + log inspection + vision check is the actual gate.
