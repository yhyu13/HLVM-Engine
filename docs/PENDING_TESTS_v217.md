# Pending Tests v217

- commit: docs/PENDING_COMMIT_v217.md
- tester: agent_5_tester (tick-565)
- timestamp: 2026-08-21
- mode: FILE-ONLY. Nothing built, run, compiled, linted, or viewed. No runtime result is claimed.

## Discipline applied

The rule this cycle produced, applied to this cycle's own verification: **no `|` alternation; no
`file_glob`; every load-bearing zero paired with a same-shape, same-scope positive control; every count
re-derived here rather than inherited from the commit marker.**

## Verifier rows

| # | Row | Query | Result | Verdict |
|---|---|---|---|---|
| 1 | terminal blocked, foreground | `terminal command="true; echo PROBE_OK; pwd"` | `pending_approval` / `tirith:unknown` / `-1` / `smart_denied false` | PASS |
| 2 | terminal blocked, **background** | `terminal command="echo BG_PROBE" background=true` | identical signature | PASS — net-new shape |
| 3 | timeout-as-zero, flagged | `path=/home/hangyu5 pattern="tirith_fail_open"` | `0` + `truncated:true, limit_reason:search_timeout` | PASS |
| 4 | timeout-as-zero, **unflagged** | `path=~/.hermes pattern="tirith"` | `0`, no flag | PASS |
| 5 | **control for 4** | `path=~/.hermes/config.yaml pattern="tirith"` | **4** → `:484 :485 :486 :487` | PASS — row 4 is a false zero |
| 6 | `file_glob` suppression | `path=~/.hermes file_glob="config.yaml" pattern="redact_secrets"` | `0` | PASS |
| 7 | **control for 6** | `path=~/.hermes/config.yaml pattern="redact_secrets"` | **1** → `:483` | PASS — row 6 is a false zero |
| 8 | `file_glob` suppression, 2nd | `path=~/.hermes/cron file_glob="*.json" pattern="enabled_toolsets"` | `0` | PASS |
| 9 | **control for 8** | `path=~/.hermes/cron/jobs.json pattern="enabled_toolsets"` | **4** → `:43 :88 :134 :180` | PASS |
| 10 | v216's control re-run | `path=~/.hermes/cron pattern="enabled_toolsets"` | **0** (v216 recorded **28**) | PASS — v216's control is dead |
| 11 | negative control, file scope | `path=~/.hermes/cron/jobs.json pattern="ZZZ_NO_SUCH_TOKEN"` | `0` | PASS — file scope not matching indiscriminately |
| 12 | dir scope OK outside project root | `path=/usr/bin pattern="git*" target=files` | **4** | PASS — falsifies rev-1's scope theory |
| 13 | `tirith` absent, controlled by 12 | `path=/usr/bin pattern="tirith*" target=files` | `0` | PASS |
| 14 | no per-job `cron_mode`, controlled by 9 | `path=~/.hermes/cron/jobs.json pattern="cron_mode"` | `0` | PASS |
| 15 | `approvals` block, direct read | `read_file ~/.hermes/config.yaml offset=460` | `:472/:473 manual/:474 60/:475 allow` contiguous | PASS |
| 16 | `security` block, direct read | same | `:481/:484 true/:485 tirith/:487 true` contiguous | PASS |
| 17 | job `c6abd4d5fc39` live | `read_file ~/.hermes/cron/jobs.json` | `:122 enabled:true` `:123 scheduled` `:134-137 ["terminal","file"]` `:120 completed:3545` | PASS |
| 18 | no concurrent tick | `path=<root> pattern="*.lock" target=files` | **5** hits, none `.pipeline.lock` | PASS — controlled |
| 19 | v217 markers written | `path=docs pattern="PENDING_*_v217.md" target=files` | 3 at review time | PASS |
| 20 | zero engine source modified | `files:` list contains only `docs/` paths | no `.cpp`/`.h`/`.hlsl`/`.py`/`.sh` | PASS |

## The row nobody specified — I re-checked the CARD's own premise

Every marker since tick-526 asserts the GBuffer SRV binding premise is refuted, and 39 cycles have
inherited that. Given rows 3-10, an inherited claim is exactly what should not be trusted. Re-derived
first-hand, file-scoped:

- `path=FGIPass.cpp pattern="SetBindingOffsets"` → **1** at `:326` — `SetBindingOffsets(0, 0, 0, 0)`,
  the NVRHI 256-default gotcha explicitly handled (`AGENTS.md §NVRHI Rendering`).
- `path=FGIPass.cpp pattern="SetTextureSRV"` → **3** at `:608 (1, Desc.GBufferWorldPos)`,
  `:609 (2, Desc.GBufferNormal)`, `:610 (3, Desc.GBufferMaterial)`.
- `path=<GI dir> pattern="AddTextureSRV"` → **3**, correct `Add*` builder verbs.

**Layout↔set pairing is N-for-N with real textures, and the offsets are zeroed.** The card's premise
remains refuted on first-hand evidence, not inherited. Note the line numbers have MOVED since tick-527
recorded `:306-308` / `:583-585` — they are now `:326` / `:608-610`, consistent with the v183-v216 source
patches. **The counts are invariant; the line numbers were not.** Any future row citing `:583` would
have been a false negative.

**20/20 PASS + 1 unspecified row PASS.**

## Not tested, and not testable from here

Acceptance gates 1-7 (build; `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8`; VUID/ERROR sweep on a fresh log;
command-list errors; `validate_restir_gi.py` on the newest dump group; vision on a fresh display PNG;
mode-20 non-zero `GBufferMaterial`) all require `terminal`, blocked per rows 1-2 in both invocation
shapes. Gate 6 additionally requires an image tool absent from this runspace. **No gate is claimed PASS.**
