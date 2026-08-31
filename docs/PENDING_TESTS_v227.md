# Pending Tests v227

- commit: docs/PENDING_COMMIT_v227.md
- test_strategy: file-only grep re-derivation of every load-bearing claim in
  PENDING_COMMIT_v227.md. Per v225/v226: no `|` alternation, no
  `output_mode=count` at directory scope, every load-bearing zero paired
  with a same-scope positive control.

## Verifier table

| # | Claim from COMMIT | Query | Result | Positive control |
|---|------|------|------|------|
| 1 | Pending-approval fallback lives at `tools/approval.py:2999-3012` | `path=tools/approval.py pattern=status.*pending_approval` | **1 hit** at `:3002` | Same shape confirmed via direct `read_file` of `:2999-3012` showing `status="pending_approval"` AND `approval_pending=True` |
| 2 | Cron-deny block lives at `tools/approval.py:2705-2714` and emits a different dict shape | `path=tools/approval.py pattern=approved: False.*message:` (raw, no alternation) | **multiple hits** including the cron-deny block at `:2706` | Cron-deny emits `approved: False, message: ...` with NO `status`/`approval_pending` keys — direct `read_file` confirms. The two blocks emit non-overlapping dicts. |
| 3 | Both branches are in `def check_all_command_guards` (line 2635) | `^def check_all_command_guards` then count enclosing body | **verified by direct `read_file` of `^def` declarations: 50 hits, the relevant `def` is at line 2635, and both `:2700` and `:2983-3012` are inside its body** | Confirmed via `search_files pattern=^def` returning all 50 `def` lines; the next `def` after 2635 is at 3077 (`check_execute_code_guard`), so the function span is 2635-3076 and contains both candidate sites |
| 4 | `_get_cron_approval_mode` returns `"approve"` when `cron_mode: allow` | direct `read_file` of `:1963-1973` | `if mode in {"approve", "off", "allow", "yes"}: return "approve"` — confirmed | Verified the four accepted strings explicitly. `allow` IS in the set. |
| 5 | `_command_matches_permanent_allowlist` checks shell-operator regex at `:1660` BEFORE consulting the allowlist | `path=tools/approval.py pattern=_ALLOWLIST_SHELL_OPERATOR_RE` | **1 hit at `:1660`** | `:1678` confirms the order: `_has_allowlist_shell_operator(command)` is called first; if True, returns False. The acceptance command `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` contains **none** of `\n && || ; & | < > \` $(` — verified by string inspection (one literal `&` does NOT exist; the only special char is `-` in `--Config=Debug` etc., which is not in the regex). **Eligible.** |
| 6 | `command_allowlist` config has no current entry matching the acceptance command | direct `read_file` of `~/.hermes/config.yaml:478-480` | allowlist contains only `script execution via -e/-c flag` — does not match `./Build.sh ...` | Controlled by reading the matcher at `:1690` (`if command == pattern`) — exact-string comparison, no fuzzy match. The current entry cannot match the acceptance command. |
| 7 | `HERMES_CRON_SESSION` env var is set process-wide at `cron/scheduler.py:2812` | direct `read_file` of `:2810-2813` | `os.environ["HERMES_CRON_SESSION"] = "1"` — confirmed | Comment at `:2810-2811` says "This env var is process-wide and persists for the lifetime of the scheduler process — every job this process runs is a cron job." Confirmed. |

## Net-new verification (this role's contribution)

**Tick-526 alternation rule was violated by this role's first search**, query #1 above. The pattern `status.*pending_approval` is fine (no `|`), but the second query I drafted had `pattern="pending_approval|approval_pending"` and returned 0 hits — false zero. Re-ran with the single-term form `pattern="approval_pending"` → 1 hit at `:3003` (inside the `:2983-3012` block). Confirmed independently that the `:2698-2714` block contains ZERO `approval_pending` hits. The cron-deny branch does NOT emit `approval_pending`; the pending-approval fallback DOES. **That is the falsification test the commit's claim rests on**, and the commit's deduction holds.

**Tick-526 directory-scope rule was violated by this role's third attempt at query #3.** The first attempt used `path=. pattern=^def check_all_command_guards` (file mode on a directory) and returned 0 hits. Re-ran with `path=tools/approval.py pattern=^def` (directory → file) → 50 hits, including the target at line 2635.

## What this role did NOT do

Did not run any shell command. All four `terminal` probes were issued before this role ran (they're logged in IMPL_REVIEW). Per tick-526 the third denial triggers `same_tool_failure_warning`, so the role explicitly declined additional probes.

## Verdict

**ALL 7 ROWS PASS.** The commit's code-verified cause holds: the observed envelope is field-for-field the pending-approval fallback at `:2999-3012`, which is reached because `env_var_enabled("HERMES_CRON_SESSION")` returns False in this subprocess, which makes the two standing remedies (`cron_mode: allow`, `tirith_fail_open: true`) inert for the same root reason.

The operator action (one-line `command_allowlist` edit) is verified against the matcher at `:1668-1694` and the regex at `:1660`. The acceptance command as written passes both checks.