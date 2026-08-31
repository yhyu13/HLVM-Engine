# Pending Commit v222

- plan: docs/PENDING_PLAN_v222.md
- files: docs/PENDING_PLAN_v222.md, docs/PENDING_PLAN_REVIEW_v222.md, docs/PENDING_COMMIT_v222.md,
  docs/PENDING_IMPL_REVIEW_v222.md, docs/PENDING_TESTS_v222.md, docs/PENDING_TEST_AUDIT_v222.md,
  docs/PIPELINE_HEALTH_2026-08-21_six-role-tick-570.md, docs/PENDING_PICK.md
- source: no bundle — direct source read
- target: no branch; nothing committed
- task: Derive the terminal-refusal envelope from running source. **Result: the frame shared by
  v215-v221 is wrong, and the standing operator remedy would not have worked.**
- verify: operator runs the single command in the audit doc
- skip_impl_review: no
- produces_test_files: no
- notes: **ZERO engine source touched. Zero agent source touched. Zero config touched.**

## Finding 1 — the tirith binary EXISTS. v215's central negative is false.

`search_files target=files path=~/.hermes pattern="tirith*"` → **1 hit:
`/home/hangyu5/.hermes/bin/tirith`.** Positive control: the same query on the same directory
returns `uv` and `uvx` alongside it, so the tree is being read.

v215 concluded "a binary that does not exist (0 hits in `/usr/bin`, `/usr/local/bin`, `/opt`,
`~/.local`, `~/.hermes`)". **It searched the right directory and got a false zero**, because
content-mode `search_files` silently returns 0 under `~/.hermes` — the exact hazard v219 recorded
and that this cycle re-demonstrated: `command_allowlist` → 0 hits by search, but `read_file` shows
it plainly at `config.yaml:478-479`. `target=files` is a different code path and it finds the
binary.

**554 ticks of "missing scanner → tirith:unknown → manual approval" rest on that false zero.**

Resolution confirmed in source, not assumed: `_resolve_tirith_path` (`tirith_security.py:685-690`)
checks `os.path.join(_hermes_bin_dir(), "tirith")` and returns it when executable — the precise file
that exists. `cfg["tirith_path"]` is the default `"tirith"` (`config.yaml:485`), so the non-explicit
branch at `:677` is the live one. No failure marker on disk (`*install*fail*` → 0 hits).

## Finding 2 — the envelope is emitted DOWNSTREAM of every config-reachable branch.

Four fields, each traced to its unique construction site:

| Field | Observed | Source | Consequence |
|---|---|---|---|
| `pattern_key` | `tirith:unknown` | `approval.py:2824-2825` `rule_id = findings[0].get("rule_id","unknown") if findings else "unknown"`; `f"tirith:{rule_id}"` | reached ONLY inside `if tirith_result["action"] in {"block","warn"}` (`:2822`) |
| `description` | `Security scan: security issue detected` | `:2500-2502` — the **`if not findings`** branch | findings list is EMPTY |
| `status` | `pending_approval` | `_await_gateway_decision` / prompt path, below `:2836` | a warning was appended, so `warnings` is non-empty |
| `allow_permanent` | `true` | `:2912` `not has_tirith and not smart_denied_for_owner` | — |

`tirith:unknown` with **empty findings** is only producible one way: `check_command_security`
returned action `block` or `warn` with `findings == []`. Reading the function's every return
(`tirith_security.py:744-854`), the returns carrying `findings: []` **and** a non-allow action are
exactly the fail-**closed** ones (`:772`, `:795`, `:805`, `:824`) — plus the JSON-parse-degraded
path at `:837-838`, whose summary is `"security issue detected (details unavailable)"`.

The observed description is `"Security scan: security issue detected"` — no `(details unavailable)`
suffix. That is `approval.py:2501`'s `or "security issue detected"` **default**, which fires only
when `summary` is ALSO empty. Of the fail-closed returns, every one sets a non-empty summary.
**So the emitting return is one whose summary is empty and whose action is block/warn** — and the
scanner is present and runs.

**The decisive structural point, independent of which return fires:** `:2822` and `:2825` sit in
Phase 1/2, **below** `:2698`. Reaching them requires `is_cli or is_gateway or is_ask` to be TRUE —
because `:2698`'s `if not is_cli and not is_gateway and not is_ask` returns approved at `:2762`
otherwise, and the cron-mode block nested inside it never emits a `pattern_key` at all.

## Finding 3 — therefore the standing remedy set is REFUTED, both members.

- **`approvals.cron_mode`** (v215/v216) — inside the `:2700` branch, which this session never
  reaches. Confirmed inert; v216 already found it was `allow` and changed nothing.
- **`approvals.mode: off`** (v220/v568) — `:2686` returns approved and IS above the emitter, so it
  would suppress the prompt. But it is host-wide, and v220 recommended it while believing the cause
  was elsewhere.
- **`command_allowlist`** (v221) — `:2689` is also above the emitter, and v221's eligibility
  analysis of the bare command is correct (re-verified: no `\n`, `&&`, `||`, `[;&|<>\``, or `$(`).
  It requires an operator to ADD the entry; `config.yaml:479` holds one unrelated entry.

**The real cause is `HERMES_EXEC_ASK`.** `gateway/run.py:1791` sets it to `"1"` at module scope,
col 0, unconditionally — so `is_ask` is true in this process, `:2698` is false, and control falls
through to the scanner phase. v220 identified this line; **what no cycle established is that it is
the operand that makes the tirith emitter reachable at all.** Config cannot switch it off.

## Plan Deviations

None. The plan's three binding additions from the review gate were all executed; addition 1 produced
the cycle's headline finding, which is the strongest argument yet for the plan-critic gate.

**Not done: nothing built, run, compiled, linted, validated or viewed. Acceptance gates 0/7.**
