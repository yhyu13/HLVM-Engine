# Pending Plan v221

- task: Re-derive v220's approval-gate finding first-hand; test execution-mode degrees of freedom
  never tried in 568 ticks; settle whether the `command_allowlist` remedy was correctly rejected.
- source: no bundle — direct read of `/home/hangyu5/Documents/Gitrepo-My/hermes-agent/tools/approval.py`
  (3390L), `gateway/run.py`, `~/.hermes/config.yaml`, `~/.hermes/cron/jobs.json`
- approach: v220 established that `check_dangerous_command`'s cron branch (`:2700`) is gated behind
  `not is_cli and not is_gateway and not is_ask` (`:2698`) and that `is_ask` is unconditionally true
  because `gateway/run.py:1791` sets `HERMES_EXEC_ASK=1` at module scope. That is inherited, not
  verified. This cycle re-derives it, then extends in two directions no prior tick took:
  (1) **execution-mode probes** — every one of ~568 prior `terminal` denials used the default
  foreground shape. `background=true` and `pty=true` are separate code paths and were never tried.
  (2) **re-open v220's rejection of the `command_allowlist` remedy**, which rested on the compound
  command guard at `:1660`. Whether the acceptance command must be compound was never checked.
- diff_estimate: +0 / -0 engine source. Markers and audit only.
- skip_plan_review: no
- test_strategy: role #5 re-runs every load-bearing query independently, at directory scope, with a
  same-shape positive control for each zero, and reads each load-bearing line as a contiguous range.
- risks:
  - Inheriting v220's operand attribution instead of re-deriving it. Mitigated: read `:2692-2700`.
  - `search_files` content mode is vacuous under dotfile directories (v219). Mitigated: `read_file`
    for everything under `~/.hermes`, with `target=files` as the controlled cross-check.
  - Concluding "the allowlist cannot work" from v220 without checking the compound premise.
