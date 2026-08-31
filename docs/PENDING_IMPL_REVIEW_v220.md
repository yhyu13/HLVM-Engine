# Pending Impl Review v220

- plan: docs/PENDING_PLAN_v220.md
- commit: docs/PENDING_COMMIT_v220.md
- verdict: KEEP (with one load-bearing correction applied here, and one severity upgrade)
- reviewer: agent_4_reviewer (tick-568)
- timestamp: 2026-08-21

## plan_fidelity_check

The impl matches the plan and executed both binding additions from the plan gate. Addition 1 was
answered honestly rather than favourably — the impler stated that `check_code_execution` is live code
but was never called by this session, so the differential is an intent argument, which is the weaker
of the two readings it could have claimed. Addition 2 changed the cycle's output, which is what a
binding addition is for. No undeclared deviations.

## CORRECTION — the impler's disqualifier (ii) is FALSE, and I re-derived it rather than read it

The commit rejects the `command_allowlist` remedy on two grounds. **Ground (ii) does not hold.** It
claimed `load_permanent_allowlist()` is never called on the cron path, evidenced by
`search_files path=cron` → 0 hits against 13 hits in `tools/approval.py`.

Re-run at file scope: `search_files path=tools/approval.py pattern="load_permanent_allowlist"` → **2
hits**, not 13. The impler's "13" was the count for a *different, broader* query it had run earlier in
the cycle and carried forward — precisely the "counts are not invariants, sets are" error the v217
audit named and the v219 audit caught again in its own tester. Third occurrence; it is now the most
recurrent defect in this pipeline's own process, not in the code it studies.

The two hits are the definition (`:1702`) and **`:3390` — a bare module-scope call, the last line of
the file, commented `"Load permanent allowlist from config on module import"`.** So the allowlist IS
populated in any process that imports `tools.approval`, which necessarily includes this one, since the
refusal we received was constructed inside that module. **The loader's reachability is not
cron-path-dependent at all.**

**The remedy's rejection nevertheless STANDS, on ground (i) alone**, which I verified independently:
`_ALLOWLIST_SHELL_OPERATOR_RE` (`:1660`) matches `&&`, and `:1678` returns False for any command
containing it. The acceptance command is `cd ... && ./Build.sh ...`. A compound command can never
match the allowlist, whatever it contains. **Right answer, one wrong reason — corrected here rather
than by rewriting the commit marker, per the standing rule that a closed marker is not rewritten.**

This correction matters beyond bookkeeping: had ground (i) not existed, the cycle would have rejected
a *working* remedy and recommended a strictly more dangerous one.

## SEVERITY UPGRADE — the divergence is a defect in the agent, not merely a discriminator

The commit uses the `check_dangerous_command` / `check_code_execution` asymmetry as an epistemic
instrument and stops there. It is also a finding in its own right, and the stronger one:

`execute_code` — which the agent's own block message at `:3126-3128` describes as *"arbitrary local
Python (including subprocess calls that bypass shell-string approval checks)"*, i.e. the **more**
dangerous surface — consults cron policy unconditionally at `:3121`. `terminal`, the **less** dangerous
surface, consults it only under a three-operand conjunction at `:2698`. The safety ordering is
inverted relative to the two functions' own stated risk assessment of each other.

I am recording this and **not acting on it**: a cron job must not patch the agent executing it. It is
reported for the operator, and it is the substantive reason `approvals.cron_mode` has never worked here.

## Security scan

- [x] No secrets read, written, or logged. `config.yaml` was read at `:468-489` only.
- [x] No shell injection — nothing executed; `terminal` refused at every invocation shape.
- [x] No eval/exec — none introduced; `execute_code` was read, never called.
- [x] No SQL.
- [x] **No modification of agent source.** Verified: the cycle's `files:` is empty and no `patch`/
      `write_file` targeted `hermes-agent` or `~/.hermes`.
- [x] No governance file touched. No commit, no push.

## Self-review checklist

- [x] Validation: every load-bearing line re-read at an explicit offset as a contiguous range, not via
      grep hits. `:2649-2690`, `:2686-2700`, `:3106-3136`, `:1660-1694`, `:1702-1717`, `:1887-1912`.
- [x] Error handling: the `mode: off` recommendation was checked against the bool-parsing path
      (`:1899-1900`) so a bare unquoted `off` cannot silently become `manual`. Confirmed: `False → "off"`.
- [x] Tests: none produced; `produces_test_files: no` is accurate, so the reviewer was still required
      and ran (HARD INVARIANT #2 satisfied, not bypassed).

## Feedback for impler (FIX only)

n/a — KEEP. The correction and the upgrade are recorded above and carry into the audit.
