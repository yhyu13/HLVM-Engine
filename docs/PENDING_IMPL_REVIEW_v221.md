# Pending Impl Review v221

- plan: docs/PENDING_PLAN_v221.md
- commit: docs/PENDING_COMMIT_v221.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-569)
- timestamp: 2026-08-21

## plan_fidelity_check

Both plan items executed; no deviation declared and none found. The plan gate's added requirement
(read the sibling operands) was honoured and produced FINDING 3. Scope held: zero engine source
touched, nothing built or run, no governance file modified.

## What I re-derived rather than accepted

**FINDING 2 is the one I tried hardest to break, because it contradicts the previous cycle.** The
claim is that the acceptance command contains no character in `_ALLOWLIST_SHELL_OPERATOR_RE`. Read
the regex at `:1660` character by character — `\n`, `&&`, `\|\|`, and the class `[;&|<>\`]`, plus
`\$\(`. Then the candidate string, token by token: `./Build.sh`, `--Config=Debug`,
`--Target=TestReSTIR_GI_Temporal`, `--Test`. The `=` is not in the class; `.` and `/` are not; the
hyphens are not. **No match. The command is eligible.** v220's contrary conclusion was about a
different string than the one that has to be run, and the difference is a `cd` that `workdir`
already performs.

I also checked the direction of `:1678-1679` — it returns False (ineligible) *on* match, so a
non-matching command falls through to the pattern loop. Correct polarity; the impler did not invert it.

**FINDING 3's ordering claim.** Verified `:241-242` is the *first* statement in
`_is_gateway_approval_context`'s body, above both `HERMES_GATEWAY_SESSION` (`:243`) and the platform
fallback (`:245`). Ordering matters to the claim: had the cron check come last, a gateway-platform
cron could still be classed as gateway and the "explicitly hardened" reading would weaken. It comes
first. **Confirmed.**

**The asymmetry argument.** The impler infers oversight from one operand being hardened and its
neighbour not. That is an inference about intent, and I pushed on it. It survives on the docstring:
the author did not merely guard cron, they wrote down *the exact failure mode* — indefinite block on
a listener-less pending approval — that the unhardened operand then delivers. Knowing the failure,
guarding one path against it, and leaving the adjacent path in the same conjunction unguarded is an
oversight on any reading. **Upheld, and it is stronger than v220's framing.**

## Security scan

- [x] No hardcoded secrets — markers contain no credentials
- [x] No shell injection — nothing executed
- [x] No eval/exec
- [x] No SQL injection
- [x] **Agent source NOT modified** — `tools/approval.py` and `gateway/run.py` were read only. A cron
      job must not patch the agent executing it.
- [x] **`~/.hermes/config.yaml` NOT modified** — read only. Both remedies are reported for the
      operator; an agent widening its own permissions is precisely what `_HERMES_CONFIG_PATH`
      (`:262-275`) exists to prevent, and that comment says so in as many words.

## Self-review checklist

- [x] Validation: every load-bearing line read as a contiguous range, not as a bare grep hit
- [x] Error handling: the three-shape probe result is reported as *upholding* the prior claim, not as
      a reversal — the impler resisted overstating a negative result
- [x] Tests: role #5 re-runs each row independently

## Feedback for impler (FIX only)

n/a — KEEP.
