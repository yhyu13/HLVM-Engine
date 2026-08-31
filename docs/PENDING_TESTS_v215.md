# Pending Tests v215

- commit: docs/PENDING_COMMIT_v215.md
- tester: agent_5_tester (tick-563)
- mode: file-only verifier (no build, no run, no image viewed — terminal unavailable)
- rule applied: every row states the SCOPE it queried at (reviewer's instruction); every zero carries a
  same-shape positive control (v205); no `|` alternation in any pattern (tick-526)

| # | Claim | Query + SCOPE | Result | Verdict |
|---|---|---|---|---|
| 1 | `jobs.json` exists | `read_file ~/.hermes/cron/jobs.json` | 188 lines, 12,935 B | PASS |
| 2 | Lineage searched wrong scope | `search_files pattern="jobs.json" path=docs/PENDING_PICK.md` | 10 hits, all prose asserting absence; 0 report a found path | PASS |
| 3 | Two jobs enabled | `pattern='"enabled": true' path=~/.hermes/cron/jobs.json` | 2 hits (122, 168) | PASS |
| 4 | Two jobs scheduled | `pattern='"state": "scheduled"' path=~/.hermes/cron/jobs.json` | 2 hits (123, 169), adjacent to row-3 hits | PASS |
| 5 | Jobs grant terminal | `read_file` job objects | all 4 declare `enabled_toolsets: ["terminal","file"]` | PASS |
| 6 | This session IS job `c6abd4d5fc39` | compare stored `prompt` to session instruction | byte-identical incl. all 7 gates + DIAGNOSTIC ref | PASS |
| 7 | `tirith` binary absent | `pattern="tirith"` at `/usr/bin`, `/usr/local/bin`, `/opt`, `~/.local`, `~/.hermes` | 0 hits each | PASS (controlled by row 8) |
| 8 | Row-7 zero is real, not a broken query | same shape: `pattern="git" path=/usr/bin` | 1 hit `/usr/bin/git` | PASS |
| 9 | approvals require a human | `pattern="mode: manual" path=~/.hermes/config.yaml` | `:473 mode: manual`, `:474 timeout: 60` | PASS |
| 10 | scanner fails open yet emits unknown | `pattern="tirith_fail_open"` same file | `:487 true`; observed `pattern_key: tirith:unknown` | PASS |
| 11 | cron actually fires | `~/.hermes/cron/ticker_heartbeat` + `output/c6abd4d5fc39/2026-08-21_15-14-27.md` | heartbeat live; transcript 2,056 lines | PASS |
| 12 | No source file touched this cycle | files list in COMMIT marker | 6 docs, 0 source | PASS |

## Limitation (stated, not hidden)

Rows 1-12 are all **file-only**. Not one of the seven acceptance gates is exercised here: nothing was
built, run, compiled, validated, or viewed. This cycle changes the *diagnosis of why* the gates are
unreachable; it does not reach them. A future cycle that cites this marker as evidence the pipeline works
has misread it.
