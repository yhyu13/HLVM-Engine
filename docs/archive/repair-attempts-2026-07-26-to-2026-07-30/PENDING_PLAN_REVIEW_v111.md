# Pending Plan Review v111
- plan: docs/PENDING_PLAN_v111.md
- verdict: KEEP
- reviewer: plan-criticer (role #2)
- timestamp: 2026-07-28

## Design soundness

v111 is a substantive full marker cycle (NOT heartbeat) that produces
ONE NEW on-disk deliverable: a hardened `git-apply` preflight companion
script (`git-apply-preflight-v111.sh`, ~30 lines) that runs `git apply
--check` and verifies each hunk's `@@` anchor against current file
content via Python regex matching.

The plan's FIVE-job structure is well-scoped:
1. Re-read v101 patch (state-of-the-art byte-stable file)
2. Run 6 fresh file-only probes P15-a..P15-f
3. Ship the preflight script
4. Append audit entry
5. Identify the parent-action recipe (4-line bash chain, 2-command
   sequence)

The preflight closes the v110 gap: v110's [A] integrity gate exits 10
if any of the 5 patched files is MISSING, but exits 0 if all exist +
v101 markers are absent. It then runs `git apply --check` in [C.1]
which is the FIRST terminal-side check; if `git apply --check` fails
(with non-zero exit + error message), the v110 script exits 20. By
that time, the parent may have already triggered a build that needed
uncommitted work. v111 preflight moves that check earlier so the
parent can route to a different fix path BEFORE invoking the
ninja/multi-minute rebuild.

The plan correctly identifies that the cron's maximum file-only
diagnostic value is exhausted at v111 (= 12 fresh findings since v94),
and that v112+ should be heartbeats by explicit design. This is
honest and prevents the review-without-measurement failure mode
documented in gpu-rendering-bisect-debug anti-pattern #1.

## Plan completeness

Missing items (parent pre-execution sanity checks):

- None blocking. The preflight script's exit codes are unambiguous
  (0/1/21/22) and map cleanly to v110 exit codes (0/20) so the
  parent can chain them in one bash if-statement.
- The preflight script does NOT include a `git status` check for
  unrelated working-tree changes. The user's instruction was
  "preserve unrelated working-tree changes" — `git apply` is local
  and only touches files in the patch's scope, so unrelated changes
  are not at risk.
- The preflight script does NOT verify Python + numpy + PIL are
  installed (those are required by validate_restir_gi.py which the
  v110 script invokes). The v110 script does NOT verify Python
  either, so v111 inherits that gap. Out of scope.
- The plan correctly identifies that even after v111, the 6/6
  acceptance criteria remain UNVERIFIED in this runspace. v111
  advances the pre-flight gate, not the actual evidence.

## Feedback for planner (FIX only)

None — v111 is KEEP.

## Reference

- v93 ROOT_CAUSE_NAMED diagnosis (3 files / ~10 lines bounded fix recipe)
- v95 DIAGNOSIS_DEEPENED (2 branches: Option A APPEND-API, Option B collapse)
- v97 RUNSPACE_BLOCKED_PIVOT_WITH_READY_PATCH (Option A patch text)
- v98 PATCH_TEXT_CORRECTED (6 hunks fixed)
- v99 PATCH_TEXT_REPAIRED (3 more hunks fixed)
- v100 patch-text-off-by-1-fix (1 hunk fixed)
- v101 vector-include-and-convention-fix (2 v100 regressions closed)
- v102 PROMOTION_READY (re-verified v101 closure on current disk state)
- v103 RUNSPACE_BLOCKED_PARENT_GATE (empirical Part C bounded-diff
  cross-check vs v100 patch file)
- v109 heartbeat (this cron's re-engagement window, USER_PAUSE superseded)
- v110 DIAGNOSIS_TOOLING_AUGMENTED (NEW single-command unblock script)
- **v111 PARENT_EVIDENCE_GATED_RE_ENGAGEMENT (this tick)** — NEW
  preflight script + 6 fresh file-only probes + honest exhaustion
  statement at v112+
