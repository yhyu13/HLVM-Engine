# Pending Impl Review v217

- plan: docs/PENDING_PLAN_v217.md (revision 2)
- commit: docs/PENDING_COMMIT_v217.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-565)
- timestamp: 2026-08-21

## plan_fidelity_check

The impl matches revision 2 of the plan row for row. No `## Plan Deviations` content beyond an explicit
"none", and I agree with the impler's characterisation that revision 1's falsification at the plan gate is
the gate working, not a deviation — nothing had been implemented against revision 1.

The commit marker's 17-row table is the plan's claim set with the exact query that produced each result.
I spot-checked rows 5, 7, 9, 11, 12 and 13 by re-running them; all reproduce. Row 10 is the one that
carries the cycle and I re-ran it too: `path=~/.hermes/cron pattern="enabled_toolsets"` → **0**, where
v216's audit recorded 28 on that same tree with the same pattern. **v216's control is dead, confirmed
independently.**

## The claim I checked hardest, because it is the one that could be self-serving

The impler asserts **zero engine source modified**. Verified: `PENDING_*_v217.md` → exactly **3** files,
all under `docs/`. No `.cpp`, `.h`, `.hlsl`, `.sh` or `.py` appears in the `files:` list, and none was
written this cycle. This matters more than usual — a cycle whose entire product is "the instruments are
unreliable" has an obvious failure mode where it also quietly edits source and blames the instruments for
any resulting surprise at the first build. It did not.

## A correction I applied to my own review

My first check for a concurrent tick was `path=docs pattern=".pipeline.lock" target=files` → 0 — which is
**precisely the uncontrolled directory-walk negative this cycle condemns.** I re-ran it with a control:
`path=<project root> pattern="*.lock" target=files` → **5 hits** (yarn.lock, 3× Cargo.lock, …). The walk
reaches the tree and finds lock files, so the `.pipeline.lock` zero is real and no concurrent tick holds
the lock. **The new rule caught a defect in the review that was auditing the rule**, on its first use.

## Security scan

- [x] No hardcoded secrets — no credential-shaped literal written; `~/.hermes/config.yaml` was **read
      only**, and no value from it is reproduced here beyond the four non-secret policy flags already
      quoted in v215/v216
- [x] No shell injection — no shell executed at all (both `terminal` probes were refused)
- [x] No eval/exec
- [x] No SQL

## Self-review checklist

- [x] Validation: every zero in the commit table is paired with a same-shape positive control (rows 3/5,
      6/7, 8/9, 13/12, 14/9) — the rule is applied to the evidence for the rule
- [x] Error handling: the `search_timeout` flag is reported as observed, including the crucial detail that
      it is **absent** on the `~/.hermes` form, so the finding is "sometimes flagged" not "always flagged"
- [x] Tests: none produced; `produces_test_files: no` is accurate, so HARD INVARIANT #2 is satisfied by
      the reviewer running rather than being skipped

## What I am explicitly NOT ratifying

That any acceptance gate moved. The commit marker says so itself and I confirm it: nothing built, ran,
rendered, or was viewed. **0 of 7 gates.** The v183-v216 chain is still unbuilt. This cycle's value is
that the lineage's 564 prior negatives were produced by instruments now demonstrated to return false
zeros — including, specifically, the control v216 relied on one tick ago.

## Feedback for impler

None. Proceed to tester.
