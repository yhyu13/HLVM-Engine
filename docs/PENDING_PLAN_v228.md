# Pending Plan v228

- task: Determine WHICH approval branch actually emits the observed `pending_approval` envelope, and whether the lineage's two standing remedies sit on that path at all
- source: no bundle — direct source read of `tools/approval.py` + `~/.hermes/config.yaml` + `~/.hermes/cron/jobs.json`
- approach: v227 closed at ALL_KEEP asserting that `cron_mode: allow` and `tirith_fail_open: true` are "inert because `HERMES_CRON_SESSION` may not propagate to the agent subprocess." That is a *conjecture about an env var*, and v227 explicitly flagged the propagation link as unverified. Rather than re-assert it, this cycle decides the question **by control flow**: read `check_all_command_guards` from its top to the pending-approval fallback and determine, from the structure alone, which predicates must hold for the observed envelope to be produced. If the cron branch has an unconditional `return` at its end, then reaching the fallback *proves* we never entered the cron branch — no env-var probe needed. Also re-probe `terminal` in the bare allowlist-eligible form (tick-569 argued its eligibility from regex reading but never actually invoked it).
- diff_estimate: +0 / -0 engine source; markers + audit only
- skip_plan_review: no
- test_strategy: role #5 re-derives each load-bearing line number independently, with a same-scope positive control for every zero
- risks:
  - **tick-526 alternation bug**: no `|` in any pattern; one term per query.
  - **Dot-directory scope**: several lineage claims about `~/.hermes` rest on directory-scope searches. Ripgrep excludes hidden paths by default, so a directory-scope query under `~/.hermes` can return a *false zero*. Every claim about that tree must be made with `path=` at the FILE, or with `read_file`.
  - **Self-granting**: the likely remedy is an edit to `~/.hermes/config.yaml`'s `command_allowlist` — the file that gates this agent's own permissions. That is an operator action and MUST NOT be applied by any role in this cycle.
