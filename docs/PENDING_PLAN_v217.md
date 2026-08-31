# Pending Plan v217 (revision 2 — after plan-criticer FIX)

- task: Diagnose the false-zero mechanism that invalidates the lineage's load-bearing negatives, and replace the standing query rule with one that actually catches it
- source: no bundle — direct investigation
- skip_plan_review: no
- diff_estimate: +0 / -0 engine source lines (investigation + marker documentation only)

## Revision note

Revision 1 claimed the v199/v215/v216 query rule is "inverted outside the project root." **The
plan-criticer falsified that** with a control I had not run: `search_files path=/usr/bin pattern="git*"
target=files` → 4 hits. `/usr/bin` is outside the project root and directory scope works there. The
mechanism is not scope. Revision 2 adopts the two causes the gate demonstrated.

## Approach

**Cause 1 — silent/unflagged search timeout reported as a clean zero.**
`path=/home/hangyu5 pattern="tirith_fail_open"` → `total_count: 0` **with** `"truncated": true,
"limit_reason": "search_timeout"`. The same query at `path=/home/hangyu5/.hermes` → `total_count: 0`
with **no truncation flag**, while `path=<the config.yaml itself>` → **4 hits** (`:484 tirith_enabled`,
`:485 tirith_path`, `:486 tirith_timeout`, `:487 tirith_fail_open`). So a partial walk can surface as an
unannotated zero. Any negative taken from a directory walk over a large tree is suspect.

**Cause 2 — `file_glob` suppresses matches.** Independent of timeouts:
`path=~/.hermes file_glob="config.yaml" pattern="redact_secrets"` → 0 (the token is on `:483`);
`path=~/.hermes/cron file_glob="*.json" pattern="enabled_toolsets"` → 0 (file-scoped → 4, at
`:43/:88/:134/:180`). Recorded once at tick-526, then not applied for 39 cycles.

**Replacement rule (this is the deliverable).** Not "prefer files" and not "prefer directories" — both
fail. **A load-bearing negative requires a same-shape, same-scope positive control, and must never use
`file_glob`.** Only the paired control distinguishes "absent" from "not looked at". This subsumes
tick-526's no-alternation rule and v199/v215's scope rule, both of which were special cases fixed to the
wrong variable.

## What is re-derived under the corrected shape

| Claim | Old basis | Status |
|---|---|---|
| `approvals.cron_mode: allow` `:475` | file-scoped | SURVIVES |
| `security.tirith_fail_open: true` `:487` | file-scoped | SURVIVES |
| block membership of both | file-scoped, 3 shapes | SURVIVES |
| `tirith` absent from `/usr/bin` | dir-scoped **with control** | SURVIVES — `tirith*` → 0 vs `git*` → 4 |
| **no per-job `cron_mode` override** | dir-scoped, control now returns 0 | **DEAD — re-derive** |
| v216 control `enabled_toolsets` → "28 hits" on `~/.hermes/cron` | dir-scoped | **DEAD — now returns 0** |

Re-derivation of the dead row, file-scoped with a same-file control:
`path=~/.hermes/cron/jobs.json pattern="cron_mode"` → **0**; control `pattern="enabled_toolsets"` → **4**.
Conclusion unchanged (no per-job override; global `allow` operative) but now supported.

## Net-new, independent of the above

`terminal` refused twice this tick in two different invocation shapes — foreground
(`true; echo PROBE_OK; pwd`) and **background** (`echo BG_PROBE`). Both: `pending_approval` /
`tirith:unknown` / `exit_code -1` / `smart_denied false` / `allow_permanent true`. **No prior tick
probed the background shape.** The block is invocation-shape-independent.

## test_strategy

Every row re-run by the tester under BOTH scopes, each paired with a same-shape control, none using
`file_glob`. Any zero without a passing control is recorded UNSUPPORTED rather than PASS.

## risks

- Over-correcting to "always use a file" breaks the v199 case, which was real. The rule must key on the
  control, not the scope.
- The `tirith`-absent claim must NOT be weakened; it is controlled and sound.
- Nothing here touches engine source. The v183-v216 chain remains unbuilt; no acceptance gate moves.
