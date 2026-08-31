# Pending Impl Review v222

- plan: docs/PENDING_PLAN_v222.md
- commit: docs/PENDING_COMMIT_v222.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-570)
- timestamp: 2026-08-21

## plan_fidelity_check

Matches the plan, executes all three binding review additions, and declares no deviations — correctly,
because there are none. The plan asked for a source-derivation of the envelope and a ruling on whether
the standing remedy targets the producing path; both delivered. Addition 1 (re-derive the binary
negative with a different query shape) produced the cycle's headline, which is the plan-critic gate
paying for its own latency for the second cycle running.

## The commit UNDER-CLAIMS, and I am upgrading it — I found the exact emitter

The impler pinned the emitting *region* structurally (below `:2698`, inside the tirith warn/block
branch) and then argued from field semantics. Correct, but it left one field unreconciled and did not
flag it. **I re-derived rather than accepted, and the loose end resolves the site exactly.**

The loose end: `allow_permanent` is computed at `:2912` as `not has_tirith and not
smart_denied_for_owner`, and `has_tirith` (`:2881`) is true whenever any warning is a tirith warning —
which is precisely our case. That expression yields **False**. **We observe `allow_permanent: true`.**
Taken at face value this refutes the impler's own account.

It does not, and the reason locates the branch: `:2912` belongs to the **gateway-callback** path. Our
call goes down the fallback at `:2983`, whose comment names our case in its first line — *"no gateway
callback registered (e.g. cron, batch)"*. There, `allow_permanent` is never set at all except under
smart-deny (`:3011`), so it is absent from the dict and surfaces as the caller's default `true` via
`terminal_tool.py:2357`'s `approval.get("smart_denied", False)`-style read at `:2356-2357`.

**The result dict at `:2999-3012` matches all five observed fields simultaneously and uniquely:**
`approved: False`, `status: "pending_approval"`, `approval_pending: True`, `pattern_key: primary_key`,
`description: _disp_combined_desc`. No other return in the module carries `status` and
`approval_pending` together. **The emitter is `approval.py:2999-3012`, not merely "somewhere below
:2698".**

This strengthens the commit's conclusion rather than weakening it: `:2983` is **below** `:2698`,
`:2686` and `:2689`, so every structural claim stands, now with the site named.

## Security scan

- [x] No hardcoded secrets — no secrets in any marker
- [x] No shell injection — no code produced; nothing executed
- [x] No eval/exec — none
- [x] No SQL — none
- [x] **Did not modify `tools/approval.py`, `gateway/run.py`, `tirith_security.py`, or
      `~/.hermes/config.yaml`.** A cron job must not patch the agent executing it nor widen its own
      permissions. Read-only throughout. This is the correct call and the commit made it unprompted.

## Self-review checklist

- [x] Validation: every load-bearing zero paired with a positive control; the one false zero in the
      lineage (the tirith binary) found precisely because the rule was applied
- [x] Error handling: n/a, no code
- [x] Tests: role #5 re-derives independently below

## The finding that matters most, stated plainly

v215 built the lineage's causal story on "the tirith binary does not exist." **It exists.** Every
subsequent cycle — v216, v219, v220, v221 — refined the remedy while inheriting that premise without
re-testing it, because it had been recorded as established. Six cycles of genuine, careful work
downstream of one false zero, produced by the one query shape known to fail on the one directory that
mattered.

The standing rule from v541 applies with full force and should be re-read as covering markers, not
just cards: *a marker's description of evidence is evidence about the marker's author, not about the
world.* The tirith negative was five ticks old and had hardened into background fact.

## Feedback for impler (FIX only)

n/a — KEEP. The `allow_permanent` reconciliation above is an addition, not a correction; the impler's
conclusions are sound as written.
