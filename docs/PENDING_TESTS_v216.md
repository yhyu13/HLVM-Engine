# Pending Tests v216

- commit: docs/PENDING_COMMIT_v216.md
- tester: agent_5_tester (tick-564)
- timestamp: 2026-08-21
- mode: FILE-ONLY. Nothing was built, run, compiled, linted, or viewed. No runtime result is claimed.

## Verifier rows

Discipline applied: no `|` alternation (tick-526); `path` at a directory wherever a negative is load-bearing
(v199, both directions per v215); every zero paired with a same-shape positive (v205); every count
re-derived here rather than inherited from the commit marker (v211).

| # | Row | Query | Result | Verdict |
|---|---|---|---|---|
| 1 | terminal blocked | `terminal command="true"` | `pending_approval` / `tirith:unknown` / `exit_code -1` / `smart_denied false` | PASS |
| 2 | block is command-independent | row 1 used `true`, the minimal command | blocked ⇒ not shape-dependent | PASS |
| 3 | `tirith` absent `/usr/bin` | `target=files pattern="tirith" path=/usr/bin` | 0 | PASS |
| 4 | **control for row 3** | same shape, `pattern="git"` | 1 → `/usr/bin/git` | PASS — row 3's zero is real |
| 5 | `tirith` absent `/usr/local/bin` | `target=files path=/usr/local/bin` | 0 | PASS |
| 6 | `tirith` absent `~/.local/bin` | `target=files path=~/.local/bin` | 0 | PASS |
| 7 | `tirith` absent `~/.local/lib` | `target=files pattern="tirith*"` | 0 | PASS |
| 8 | `cron_mode: allow` present | `pattern="cron_mode"` | 1 → `:475  cron_mode: allow` | PASS |
| 9 | `tirith_fail_open: true` present | `pattern="fail_open"` | 1 → `:487  tirith_fail_open: true` | PASS |
| 10 | **control for rows 8-9** | same file, `pattern="ZZZ_NO_SUCH_FLAG"` | 0 | PASS — the file does not match everything |
| 11 | `approvals:` block header exists | `pattern="approvals"` | 1 → `:472` | PASS |
| 12 | `cron_mode` inside `approvals:` | `pattern="mode: manual"` context=1 | `:472/:473/:474` contiguous above `:475` | PASS |
| 13 | `tirith_fail_open` inside `security:` | `pattern="tirith_enabled"` context=14 | `:481 security:` … `:487` contiguous | PASS |
| 14 | 4 cron jobs | `pattern='"name"'` on `jobs.json` | 4 → `:5, :51, :96, :143` | PASS |
| 15 | 2 scheduled / 2 paused | `pattern='"state"'` | `:32` paused, `:77` paused, `:123` scheduled, `:169` scheduled | PASS |
| 16 | all 4 grant terminal | `pattern="enabled_toolsets"` context=3 | 4 blocks, each `["terminal","file"]` | PASS |
| 17 | this session = job `c6abd4d5fc39` | `pattern="six-role pipeline for the HLVM-Engine"` | `:97` prompt matches instruction incl. `DIAGNOSTIC_2026-07-30.md` + 7 gates | PASS |
| 18 | no interpreter code readable | `pattern="tirith_fail_open" path=~/.hermes` | 1 — the config line only | PASS (limits the conclusion) |
| 19 | no v216 markers pre-existed | `target=files pattern="PENDING_*_v216.md"` | 0 before this cycle | PASS |
| 20 | no `.pipeline.lock` | `target=files pattern=".pipeline.lock"` | 0 | PASS — no concurrent tick |

**20/20 PASS.** Row 18 is deliberately recorded as a PASS *that constrains* the finding: it is the row proving
the impler could not have verified runtime behaviour, and therefore that its refusal to claim a runtime defect
was forced by evidence rather than chosen for modesty.

## Not tested, and not testable from here

Acceptance gates 1-7 (build, `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8`, VUID/ERROR sweep, command-list errors,
`validate_restir_gi.py`, vision on a fresh display PNG, mode-20 `GBufferMaterial`) — all require `terminal`,
which row 1 shows is blocked. Gate 6 additionally requires an image tool this runspace does not have
(tick-528). **No gate is claimed PASS by this cycle.**
