# Pending Tests v219

- commit: docs/PENDING_COMMIT_v219.md
- tester: agent_5_tester (tick-567)
- mode: file-only verification. **Nothing built, run, compiled, or viewed.** `terminal` refused again
  when probed this tick (`true` → `pending_approval` / `tirith:unknown` / `exit_code -1`), which is
  itself row 1's evidence.

## Method

Every row re-derived first-hand, file-scoped, with no `|` alternation (tick-526) and with a same-shape
positive control for every load-bearing zero (v217). Rows marked **NEW** were not in the commit.

## Rows

| # | Claim | Query | Result | Verdict |
|---|---|---|---|---|
| 1 | terminal refused | `terminal command="true"` | `pending_approval`, `pattern_key: tirith:unknown`, `exit_code -1`, `allow_permanent: true` | PASS |
| 2 | cron branch gate | `approval.py` read `:2692-2762` | `:2700` gates on `HERMES_CRON_SESSION`; `:2762` returns 2-field allow | PASS |
| 3 | pending dict site | `pattern="\"approval_pending\": True"` | 2 hits: `:3003`, `:3226` | PASS |
| 4 | 7-field shape | `terminal_tool.py` `pattern="allow_permanent"` | `:2352-2358` assembles all 7 observed fields | PASS |
| 5 | `cron_mode` maps to approve | `approval.py:1968-1971` | `{"approve","off","allow","yes"}` → `"approve"` | PASS |
| 6 | config value | `path=~/.hermes/config.yaml pattern="cron_mode"` | 1 hit `:475 cron_mode: allow` | PASS |
| 7 | scheduler sets the var | `scheduler.py pattern="os.environ"` | 11 hits; `:2812` `HERMES_CRON_SESSION"] = "1"` | PASS |
| 8 | tirith installed | `target=files path=~/.hermes/bin pattern="tirith"` | **1 hit — the binary exists** | PASS |
| 9 | dotfile-dir false zero | `path=~/.hermes pattern="tirith_enabled"` → 0; `path=~/.hermes/config.yaml` same token → 1 (`:484`) | both | PASS |
| 10 | same, second token | `path=~/.hermes/cron pattern="enabled_toolsets"` → 0; `path=~/.hermes/cron/jobs.json` → 4 | both | PASS |
| 11 | no per-job cron_mode | `path=~/.hermes/cron/jobs.json pattern="cron_mode"` → 0, control `enabled_toolsets` → 4 same file | 0 with live control | PASS |
| 12 | this session is the job | `jobs.json:95-139` read | `c6abd4d5fc39`, `enabled: true`, `completed: 3547`, prompt byte-identical | PASS |
| 13 | fail_open inert here | `tirith_security.py:770-772, 793-794, 822-823` | governs only path-None / spawn-fail / unknown-exit | PASS |

## NEW rows — these resolve the disjunction the reviewer opened

The impl review left candidates (A) `HERMES_CRON_SESSION` unset and (B) `HERMES_EXEC_ASK` set, and
recorded them as inseparable from inside this session. **They are separable from source, and the
tester's job was to try before accepting.**

| # | Claim | Query | Result | Verdict |
|---|---|---|---|---|
| 14 **NEW** | scheduler never sets `HERMES_EXEC_ASK` | `path=cron/scheduler.py pattern="HERMES_EXEC_ASK"` → **0**; control `pattern="HERMES_CRON_SESSION"` same file → **1** (`:2812`) | 0 with live same-file control | PASS |
| 15 **NEW** | who DOES set it | `output_mode=files_only path=hermes-agent pattern="HERMES_EXEC_ASK"` → 26 files; non-test, non-docs setters: `gateway/run.py`, `tui_gateway/server.py`, `acp_adapter/server.py` | enumerated | PASS |
| 16 **NEW** | **`gateway/run.py:1791` sets it at MODULE SCOPE** | read `:1787-1792` | `os.environ["HERMES_EXEC_ASK"] = "1"` — top-level, not inside a function | PASS |
| 17 **NEW** | and `tui_gateway/server.py:2031-2035` sets three vars together | read | `HERMES_GATEWAY_SESSION`, `HERMES_EXEC_ASK`, `HERMES_INTERACTIVE` all `= "1"` inside `_enable_gateway_prompts()` | PASS |

**What rows 14-17 establish.** `os.environ` is process-global — the scheduler's own comment at
`:2810-2811` says so about its own write. If the cron scheduler runs **in the same process as** a
gateway (`gateway/run.py` imported → `:1791` executes at import time, unconditionally, with no cron
guard), then `HERMES_EXEC_ASK=1` is set process-wide, and `scheduler.py:2812`'s `HERMES_CRON_SESSION=1`
**does not help**, because `approval.py:2698` requires `not is_ask` as a third operand. Candidate (B)
is not merely live — **it has a concrete, unconditional, module-scope setter, and candidate (A) has
none.** `tui_gateway/server.py:2033` additionally sets `HERMES_GATEWAY_SESSION`, which would *also*
force `is_gateway` true at `approval.py:243` — reproducing (A)'s symptom without the var being unset.

This does not make (B) certain — proving it needs the process environment — but it inverts the
plausibility ordering the commit's first draft assumed, and it means the operator action must print
**both** variables, which it does.

## Rows deliberately NOT run

Build, run, dump, validator, vision. All require `terminal` (row 1) or an image tool this runspace
does not have (tick-528). **No runtime result is asserted anywhere in this cycle.**
