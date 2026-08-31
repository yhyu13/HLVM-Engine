# Pending Commit v217

- plan: docs/PENDING_PLAN_v217.md (revision 2)
- files: docs/PENDING_PLAN_v217.md, docs/PENDING_PLAN_REVIEW_v217.md, docs/PENDING_COMMIT_v217.md, docs/PENDING_TESTS_v217.md, docs/PENDING_TEST_AUDIT_v217.md, docs/PIPELINE_HEALTH_2026-08-21.md, docs/PENDING_PICK.md
- source: no bundle
- target: no branch — nothing committed, nothing pushed
- task: Diagnose two false-zero mechanisms in `search_files`; replace the lineage's standing query rule; re-derive v216's unsupported rows
- verify: (operator, at a terminal) `cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine && ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
- skip_impl_review: no
- produces_test_files: no
- notes: **Zero engine source files modified.** No `.cpp`, `.h`, `.hlsl`, `.py`, `.sh` or governance file touched. This cycle's product is a correction to the lineage's measurement instruments.

## What was established, with the query that established it

| # | Claim | Query | Result |
|---|---|---|---|
| 1 | `terminal` blocked, foreground | `terminal command="true; echo PROBE_OK; pwd"` | `pending_approval` / `tirith:unknown` / `exit_code -1` / `smart_denied false` |
| 2 | **`terminal` blocked, background — NET-NEW** | `terminal command="echo BG_PROBE" background=true` | identical signature |
| 3 | Cause 1: unflagged timeout-as-zero | `path=/home/hangyu5 pattern="tirith_fail_open"` | `0` **+ `truncated: true, limit_reason: search_timeout`** |
| 4 | same query, narrower dir, **flag absent** | `path=~/.hermes pattern="tirith"` | `0`, no truncation flag |
| 5 | control for 4 — the token exists | `path=~/.hermes/config.yaml pattern="tirith"` | **4** → `:484 :485 :486 :487` |
| 6 | Cause 2: `file_glob` suppression | `path=~/.hermes file_glob="config.yaml" pattern="redact_secrets"` | `0` |
| 7 | control for 6 | `path=~/.hermes/config.yaml pattern="redact_secrets"` | **1** → `:483` |
| 8 | Cause 2, second instance | `path=~/.hermes/cron file_glob="*.json" pattern="enabled_toolsets"` | `0` |
| 9 | control for 8 | `path=~/.hermes/cron/jobs.json pattern="enabled_toolsets"` | **4** → `:43 :88 :134 :180` |
| 10 | v216's own control now fails | `path=~/.hermes/cron pattern="enabled_toolsets"` | **0** (v216 recorded 28) |
| 11 | negative control, file scope | `path=~/.hermes/cron/jobs.json pattern="ZZZ_NO_SUCH_TOKEN"` | `0` |
| 12 | dir scope works outside project root | `path=/usr/bin pattern="git*" target=files` | **4** |
| 13 | `tirith` genuinely absent, controlled | `path=/usr/bin pattern="tirith*" target=files` | `0` vs row 12 |
| 14 | no per-job override, re-derived | `path=~/.hermes/cron/jobs.json pattern="cron_mode"` | `0`, controlled by row 9 |
| 15 | `approvals` block | `read_file ~/.hermes/config.yaml offset=460` | `:472 approvals:` `:473 mode: manual` `:474 timeout: 60` `:475 cron_mode: allow` |
| 16 | `security` block | same read | `:481 security:` … `:484 tirith_enabled: true` `:485 tirith_path: tirith` `:487 tirith_fail_open: true` |
| 17 | this session is job `c6abd4d5fc39` | `read_file ~/.hermes/cron/jobs.json` | `:122 enabled: true` `:123 state: scheduled` `:134-137 ["terminal","file"]`, `:120 completed: 3545` |

Rows 15 and 16 were taken with `read_file`, not `search_files`, deliberately: after rows 3-10 no
directory-walk negative is trustworthy without a control, and a direct read needs none.

## The rule this cycle replaces

Three standing rules — tick-526 (no `|` alternation), v199/v215 (scope the `path` at a directory) — each
fixed a *symptom variable*. Row 12 falsifies the scope rule directly. The replacement:

> **A load-bearing negative requires a same-shape, same-scope positive control, and must never use
> `file_glob`.**

The control is the only thing that separates "absent" from "not looked at", and it catches both causes
without needing to know which one fired.

## Plan Deviations

None. The plan was executed as written in revision 2. Revision 1's mechanism was falsified at the plan
gate before any work was done on it, which is the gate operating as intended rather than a deviation.

## What this cycle did NOT establish

Nothing was built, run, compiled, linted, rendered, or viewed. **All seven acceptance gates remain
unreached**, and the v183-v216 chain (33 source patches) remains unbuilt. This cycle does not move any
gate and must not be read as doing so. Its correction is to the evidence-gathering, which is why it was
worth a cycle: the lineage's negatives were produced by instruments now shown to be unreliable.
