# Pending Tests v221

- commit: docs/PENDING_COMMIT_v221.md
- tester: agent_5_tester (tick-569)
- timestamp: 2026-08-21
- test files produced: **none** (file-only cycle; nothing executable was written)

Every row re-derived independently. No count inherited from the commit or review marker.

| # | Row | Method | Result |
|---|---|---|---|
| 1 | `terminal` foreground refused | fresh probe, `date` | PASS — `pending_approval`, `exit_code -1` |
| 2 | `terminal` `background=true` refused | fresh probe, same command | PASS — identical envelope |
| 3 | `terminal` `pty=true` refused | fresh probe, same command | PASS — identical envelope |
| 4 | Bare (non-compound) build command refused | fresh probe | PASS — same envelope, so eligibility ≠ membership |
| 5 | `:2698` conjunction shape | read `:2692-2700` as a range | PASS — `not is_cli and not is_gateway and not is_ask` |
| 6 | `is_gateway` has a cron guard, first in body | read `:227-245` as a range | PASS — `:241-242`, above both other checks |
| 7 | `is_ask` has no guard | read `:2694` | PASS — bare `env_var_enabled` call |
| 8 | `HERMES_EXEC_ASK` set at module scope | `gateway/run.py:1791`, col 0 | PASS — unconditional, no `if` |
| 9 | Allowlist regex operator set | read `:1660` char by char | PASS — `\n &&  \|\| ; & \| < > \` $(` |
| 10 | Acceptance command has no operator | token-by-token against row 9 | PASS — eligible |
| 11 | `workdir` makes the `cd` redundant | `jobs.json:138` | PASS — workdir == project root |
| 12 | Allowlist check sits above the cron branch | `:2689-2690` vs `:2698` | PASS — 8 lines above |
| 13 | Allowlist config→memory wiring | `:1702-1714`, `:1681-1692` | PASS — exact match or fnmatch |
| 14 | `approvals.cron_mode: allow` present | `config.yaml:475` | PASS — correct, and unreachable |

## Controls run for the known-vacuous query shapes

- **No `|` alternation in any pattern** (tick-526). Every query single-term.
- **Dotfile-directory content search is vacuous** (v219) — re-confirmed as a *live* hazard this
  cycle, not merely honoured: `search_files path=~/.hermes/cron pattern="approv"` → **0 hits**, and
  `pattern="enabled_toolsets"` → **0 hits**, while `target=files pattern="*.json"` → **1 hit** and
  `read_file` on that same file shows `enabled_toolsets` at `:134-137`. **A content query returned
  zero against a file that provably contains the term.** Every `~/.hermes` fact in this cycle came
  from `read_file`.
- **Positive control for row 9's zero-operator finding**: `_ALLOWLIST_SHELL_OPERATOR_RE` → **2 hits**
  (`:1660` definition, `:1665` sole use). Non-zero, so the query shape works; and the 2 also
  establishes there is no second operator guard elsewhere that row 10 would have to clear.
- **Profile scope**: `target=files config.yaml` under `~/.hermes` → 3 hits, one of which is
  `profiles/claude_coder/config.yaml`. Active profile is `default` per session header, so
  `~/.hermes/config.yaml` is the operative file. Checked because a per-profile override would
  invalidate rows 14 and both remedies.

## Not tested

Nothing was built, run, compiled, rendered, validated, or viewed. Rows 1-4 are refusals, not runs.
