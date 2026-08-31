# Pending Tests v220

- commit: docs/PENDING_COMMIT_v220.md
- impl_review: docs/PENDING_IMPL_REVIEW_v220.md (KEEP + 1 correction + 1 severity upgrade)
- tester: agent_5_tester (tick-568)
- files produced: **none.** File-only verification; no test files written, nothing executed.

## Method

Every row re-derived first-hand. Per the three known `search_files` false-zero classes (alternation —
tick-526; project-root scoping — v217; dotfile directories — v219), **no `|` in any pattern, `path` at
a file or directory as appropriate, and every load-bearing zero paired with a same-shape same-scope
positive control.** Contiguous ranges read with `read_file` at explicit offsets where a claim depends
on nesting or indentation, since grep cannot show scope.

| # | Row | Method | Result |
|---|---|---|---|
| 1 | `:2698` is `not is_cli and not is_gateway and not is_ask` | `read_file :2686-2700` | PASS |
| 2 | `:2700` cron check is **nested inside** `:2698` | indentation read in same range (8 sp vs 4) | PASS |
| 3 | `:3121` cron check is at **function scope** | `read_file :3106-3136` (4 sp, no enclosing `if`) | PASS |
| 4 | `:3117-3118` computes only `is_gateway`, `is_ask` — no `is_cli` | same range | PASS |
| 5 | `HERMES_EXEC_ASK` set at module scope in gateway | `read_file gateway/run.py :1780-1804`; `:1788`/`:1791`/`:1793` all col 0 | PASS |
| 6 | `_get_approval_mode` reads `approvals.mode` | `read_file :1928-1931` → `_get_approval_config()` `:1918-1925` | PASS |
| 7 | bare `off` in YAML cannot degrade to `manual` | `read_file :1896-1912`; `:1899-1900` `False → "off"` | PASS |
| 8 | `config.yaml:473 mode: manual`, `:475 cron_mode: allow` | `read_file ~/.hermes/config.yaml :468-489` | PASS |
| 9 | allowlist rejects compound commands | `read_file :1660-1694`; `:1678` guard, `:1660` regex includes `&&` | PASS |
| 10 | **`load_permanent_allowlist` call-site set** | `path=tools/approval.py` → **2 hits** (`:1702` def, `:3390` module-scope call) | **PASS — impler's 13 was wrong; reviewer's correction confirmed** |
| 11 | `tirith` is installed | `target=files path=~/.hermes/bin pattern="*"` → 3 hits incl. `tirith` | PASS |
| 12 | **`HERMES_CRON_SESSION` call-site set (NEW)** | `path=tools/approval.py` → **4 hits**: `:241`, `:2173`, `:2700`, `:3121` | PASS — see row 13 |
| 13 | **THIRD ordering exists at `:2171` (NET-NEW)** | `read_file :2160-2183` | PASS — see below |

## Row 13 — the finding is larger than the commit or the review stated

The commit compared two sites and the review upgraded that to a defect. The cron-session call-site set
is **four**, and they exhibit **three different gate shapes** for one policy:

| Site | Guard preceding the cron check | Operands |
|---|---|---|
| `:2171` | `if not is_cli and not is_gateway:` | 2 |
| `:2698` | `if not is_cli and not is_gateway and not is_ask:` | 3 |
| `:3121` | *(none — function scope)* | 0 |

`:241` is the classifier itself (`_get_session_platform` mutual exclusion), not a policy gate.

**`:2171` is the decisive control the earlier roles lacked.** Two sites in the same file, same policy,
same comment text (`:2172` and `:2699` are both "Cron sessions: respect cron_mode config"), differing
by exactly one operand — `not is_ask`. That is not a design distinction anyone can articulate; it is
drift. And its direction is the harmful one: **the extra operand at `:2698` is what makes the terminal
path uniquely sensitive to `HERMES_EXEC_ASK`, the variable the gateway sets unconditionally at import.**

Note `:2182` `elif fail_closed_when_no_human:` — the `:2171` site has an explicit named concept for
"no human can answer." `:2698` has no such `elif`. The two sites were not written to the same contract.

## What was NOT done

Nothing built, run, compiled, linted, validated or viewed. No image inspected. No file modified in the
engine, the agent, or the config. `terminal` was probed twice this tick and refused both times
(`pending_approval` / `tirith:unknown` / `exit_code -1` / `allow_permanent: true` / `smart_denied: false`),
including on the degenerate command `date` — confirming the block is command-independent.
