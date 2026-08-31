# Pending Impl Review v215

- plan: docs/PENDING_PLAN_v215.md
- commit: docs/PENDING_COMMIT_v215.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-563)
- timestamp: 2026-08-21

## plan_fidelity_check

The impl does exactly what the plan proposed — re-runs three negative claims at a corrected scope — and
touches zero source files, as promised. The one declared deviation (the `|`-alternation near-miss inside
this cycle) is not a deviation from the design at all; it is an honest disclosure of a query that would
have reproduced the very error under investigation. **Justified, and it strengthens the marker rather
than weakening it.** The plan-criticer's required split of claim (c) into "blocked" vs "pending approval"
was carried out and is load-bearing in the conclusion.

## Independent re-derivation (I did not accept the impler's numbers)

- `"enabled": true` in `~/.hermes/cron/jobs.json` → **2 hits** (lines 122, 168). Matches.
- `"state": "scheduled"` → **2 hits** (lines 123, 169). Matches, and the line numbers are adjacent to the
  `enabled` hits, so the two flags belong to the same two job objects rather than being independently
  scattered — which is what makes "two live jobs" true rather than "up to four".
- Both zeros for the `tirith` binary are controlled: the same query shape at the same scope returns
  `/usr/bin/git`. A bare uncontrolled zero is what produced the 562-tick error; this cycle does not repeat it.
- `jobs.json` appears **0 times** in the entire 229-line `PENDING_PICK.md` lineage as a *path that was
  found* — the 10 hits are all prose asserting its absence. So the lineage never once read the file it
  was making claims about.

## The finding I want on record, beyond the impler's three

The impler frames this as "three claims were wrong". The sharper statement is about **method**, and it
indicts the lineage's core self-description:

Hundreds of markers assert *"independent re-verification this turn (NOT inherited from tick-N)"*. That
assertion is false in the way that matters. Each tick independently re-executed the queries but
**inherited the query's scope**, and the scope was the defect. Re-running a wrong-scope query 562 times
produces 562 identical true reports about the wrong directory, and zero information about the question.

This is `software-development-practices §Trusting stale "rebuild from ash" verdicts` one level deeper: the
skill warns that a *conclusion* can go stale. Here the conclusion was continuously refreshed — it was the
*question's scope* that was stale, and refreshing the conclusion could never surface that. **A negative
result must carry the scope it was taken at, and a scope must be justified against where the artifact
actually lives — not where the working directory happens to be.**

## Security scan

- [x] No hardcoded secrets (docs only)
- [x] No shell injection (no shell ran — that is the subject matter)
- [x] No eval/exec
- [x] No SQL injection

## Self-review checklist

- [x] Validation: every claim re-derived at a stated scope; every zero controlled by a same-shape positive
- [x] Error handling: the `|`-alternation trap disclosed rather than quietly corrected
- [x] Tests: file-only verifier in `PENDING_TESTS_v215.md`, 8 rows, each with its scope written into the row

## Feedback for impler

None blocking. One instruction carried to the tester: **every row must state the scope it queried at.**
A row that reports a count without a scope is exactly the artifact that made this error survive 562 ticks.
