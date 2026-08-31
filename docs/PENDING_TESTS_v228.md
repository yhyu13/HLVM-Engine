# Pending Tests v228

- commit: docs/PENDING_COMMIT_v228.md
- mode: file-only (terminal refused at tool boundary; nothing built, run, or viewed)
- tester: agent_5_tester (tick-581)
- timestamp: 2026-08-21

Every row re-run by me this cycle. No count inherited from the commit or review marker.

| # | Row | Query / read | Expected | Result |
|---|---|---|---|---|
| 1 | `:2762` terminates the non-interactive block | `read_file approval.py 2725-2794` | unconditional `return approved:True` | **PASS** — `:2762`, followed by `:2764` comment at function indent |
| 2 | Deny returns carry no `status` key | `read_file approval.py 2685-2760` | 3 two-key dicts | **PASS** — `:2705`, `:2724`, `:2750` |
| 3 | Pending envelope shape | `read_file approval.py 2985-3012` | matches observed fields | **PASS** — incl. `allow_permanent`/`smart_denied` polarity |
| 4 | Allowlist entry == description string | `pattern="script execution via"` | exact match at `:637` | **PASS** — 1 hit, byte-identical to config `:479` |
| 5 | *(negative control for row 4)* | `pattern="ZZZ_NO_SUCH_TOKEN"` same file | 0 | **PASS** — 0 hits; row 4's hit is real, not an artifact |
| 6 | `command_allowlist` present in config | `path=<config.yaml>` file scope | 1 hit `:478` | **PASS** |
| 7 | *(false-zero control for row 6)* | `path=~/.hermes` dir scope, same term | 0 hits | **PASS (bug reproduced)** — dot-dir false zero |
| 8 | *(positive control for row 7)* | `path=<project>/docs pattern="verdict: ALL_KEEP"` | non-zero | **PASS** — 10 hits; dir scope works on non-hidden trees, isolating cause to dot-prefix |
| 9 | `cron_mode: allow` | config file scope | 1 hit `:475` | **PASS** |
| 10 | `tirith_fail_open: true` | config file scope | 1 hit `:487` | **PASS** |
| 11 | tirith binary absent | `target=files pattern="tirith" path=~/home` | 0 | **INCONCLUSIVE — see note** |
| 12 | This session's job is live | `jobs.json` file scope `"enabled"` | job 3 true | **PASS** — `:122 "enabled": true`, `:123 "state": "scheduled"`, `:120 "completed": 3567` |
| 13 | Job 3 prompt == this session's instruction | `jobs.json pattern="TestReSTIR_GI_Temporal"` | match at job 3 | **PASS** — `:97`, incl. the `DIAGNOSTIC_2026-07-30.md` clause unique to this instruction |
| 14 | All jobs terminal-enabled | `jobs.json pattern="enabled_toolsets"` | 4× terminal+file | **PASS** — `:43,88,134,180` |
| 15 | No per-job `cron_mode` override | `jobs.json` file scope | 0 hits | **PASS** — sound at file scope (tick-564's version of this row was vacuous) |
| 16 | Bare allowlist-eligible command still refused | live `terminal` probe | envelope | **PASS** — `pending_approval`/`tirith:unknown`/`-1` |
| 17 | Engine source untouched | `target=files pattern="PENDING_*_v228.md"` | 4 marker files only | **PASS** — no source path in this cycle's write set |

## Note on row 11 (recorded as INCONCLUSIVE, deliberately)

I ran `search_files target=files pattern="tirith" path=/home/hangyu5` → 0 hits, and the lineage (tick-563) treats exactly this shape as proof the tirith binary is missing. **I decline to record it as PASS.** Given Finding 2 — that this tool silently skips hidden paths — a binary under `~/.local/bin` or `~/.hermes/` would be invisible to that query, and `/usr/bin` is outside the searched root entirely. The honest status is unknown from this runspace.

**This does not weaken the cycle's conclusion**, and that is worth stating: whether tirith exists is only relevant *inside* the `:2698` block, which Finding 1 proves we never enter. The row is unnecessary, not merely inconclusive. I kept it visible rather than dropping it so that a future tick does not re-derive it from tick-563 and mistake it for load-bearing.

## What I could not test

Nothing was built, compiled, executed, rendered, or viewed. No acceptance gate was exercised. All 17 rows are static file reads.
