# Pending Impl Review v219

- plan: docs/PENDING_PLAN_v219.md
- commit: docs/PENDING_COMMIT_v219.md
- verdict: FIX → (re-reviewed after correction) KEEP
- reviewer: agent_4_reviewer (tick-567)
- timestamp: 2026-08-21

## plan_fidelity_check

The impl follows the plan and both plan-gate additions. No undeclared deviations. Findings 2 and 3
are re-derived independently below and hold. **Finding 1's conclusion was over-stated and is
corrected below** — the correction does not change the remedy, which is why this closes KEEP after
the fix rather than looping.

## FIX 1 (applied) — Finding 1 asserted an exclusive disjunction that the source does not support

The commit argued: we received the seven-field dict ⇒ control reached `:2999` ⇒ the `:2698` branch was
not taken ⇒ `is_gateway` was True ⇒ (by `:241-242`) `HERMES_CRON_SESSION` is unset.

**The `:2698` condition has THREE operands, not two:** `not is_cli and not is_gateway and not is_ask`,
where `is_ask = env_var_enabled("HERMES_EXEC_ASK")` (`:2694`). And `:2887` — the block containing
`:2999` — is `if is_gateway or is_ask:`. **`is_ask` is independent of `HERMES_CRON_SESSION`;** `:241-242`
makes the var mutually exclusive with `is_gateway` only. So a session with `HERMES_CRON_SESSION` set
**and** `HERMES_EXEC_ASK` set skips the cron branch at `:2698` and lands on exactly the same `:2999`
return. The commit's inference eliminates one of two live candidates and presents the survivor as
proved.

`is_cli` is correctly excluded and the commit did not need to say so, but for completeness: an
`is_cli`-only session reaches the CLI prompt at `:3025`, which returns a choice, never the pending
dict. So the candidate set is exactly two:

- **(A)** `HERMES_CRON_SESSION` unset → session classified gateway at `:245` → `:2999`
- **(B)** `HERMES_CRON_SESSION` set but `HERMES_EXEC_ASK` also set → `:2698` skipped → `:2999`

**Both are unfalsifiable from inside this session** — separating them requires reading the process
environment, which requires the terminal under investigation. The commit must state the disjunction
and stop, per the plan gate's second addition ("if the evidence does not separate the causes, say
so rather than picking the more interesting one"). It picked one. Corrected in the audit and in the
operator action, which is written to be correct under both.

**Why this does not change the remedy:** under (A) the fix is to ensure the var is set; under (B) it
is to ensure `HERMES_EXEC_ASK` is unset in scheduler-spawned sessions. Both are "the scheduler
process's environment does not match what `:2698` expects of a cron session," and both are diagnosed
by the same one-line operator command. The operator action already prints both variables.

## FIX 2 (applied) — one word in Finding 2 over-claims

The commit says the scan "returning `block`/`warn` on the degenerate command `true` is only
explicable by a scanner that actually **ran**." Not *only* — `:2789-2805` synthesises a `warn` result
with no scanner at all when `tirith_fail_open` is false. That path is excluded here on other grounds
(`fail_open` is `true` at `config.yaml:487`, and its synthesised `rule_id` is `tirith-import-error`
whereas ours is `unknown`), but the argument as written omits the exclusion. Softened to "is not
explicable by an absent binary under this configuration," with the two exclusions stated.

## Security scan

- [x] No hardcoded secrets — no code written this cycle
- [x] No shell injection — nothing executed
- [x] No eval/exec — none
- [x] No SQL injection — n/a
- [x] **No engine source, no governance file, and no file outside `HLVM-Engine/docs/` modified.**
      `hermes-agent` was read only; a cron job must not patch the agent running it.

## Self-review checklist

- [x] Validation: Findings 2 and 3 re-derived independently by this reviewer with same-shape positive
      controls. Finding 3's controls are the strongest evidence in the cycle — a 0 and a non-0 for the
      same token differing only in `path` granularity.
- [x] Error handling: the one inference in the cycle is now labelled as a two-candidate disjunction.
- [x] Tests: file-only verification only; nothing built, run, or viewed.

## Note for the next cycle

Finding 3 invalidates a query shape, not just a query. Before any future tick records a negative from
`~/.hermes`, `~/.config`, or any other dotfile directory, it must re-derive it file-scoped. This is
the third false-zero class; the running total of vacuous negatives in this lineage is now large enough
that **any inherited negative should be treated as unproven until re-derived under the current rules.**
