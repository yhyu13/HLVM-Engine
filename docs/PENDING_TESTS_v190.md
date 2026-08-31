# Pending Tests v190

- commit: docs/PENDING_COMMIT_v190.md
- tester: agent_5_tester (tick-537)
- timestamp: 2026-08-30
- test files produced: **none** (`produces_test_files: no`)

## Why no test file

The change is comment-only, +0/-0 functional. There is no behaviour to pin. A
new test asserting on comment text would be the "test-bug-in-itself" pattern the
verifier audits for. Instead this is a **structural verifier**: each row is a
query anyone can re-run to falsify a specific claim in `PENDING_COMMIT_v190.md`.

Every query is single-term per the tick-526 alternation rule (no `|`), with
`path` at a file or directory, never a bare pattern that could return a vacuous
zero.

## Rows

| # | Claim under test | Query / read | Expected | Actual | Verdict |
|---|---|---|---|---|---|
| 1 | Nine `Bd.` assignments survive | `search_files pattern="Bd\."` | 9 | **9** at `:871-874`, `:882-886` | PASS |
| 2 | v189's fix intact | read `:882` | `Bd.OutputWidth = HalfResWidth;` | exact | PASS |
| 3 | v189's fix intact (h) | read `:883` | `Bd.OutputHeight = HalfResHeight;` | exact | PASS |
| 4 | Dispatch call present, once | `search_files pattern="BilateralDenoisePass.Dispatch"` | 1 | **1** at `:887` | PASS |
| 5 | Guard structure intact | `search_files pattern="bBypass"` | `if (!bBypass)` still opens the block | **10 hits**; `:868` `if (!bBypass)` immediately precedes `:870` `FDesc Bd{}` | PASS |
| 6 | **No stale line-number cross-refs, file-wide** | `search_files pattern="// .*:[0-9][0-9]+"` | 0 | **0** | PASS |
| 7 | New comment landed exactly once | `search_files pattern="VESTIGIAL"` | 1 | **1** at `:837` | PASS |
| 8 | No duplicate scope-limit text | `search_files pattern="Do not cite this pass as correct"` | 1 | **1** at `:867` | PASS |
| 9 | Old false mechanism gone | `search_files pattern="forces nvrhi to emit"` | only inside the quoted-and-refuted sentence | 1 hit at `:841`, inside `"..."` followed by `That mechanism is wrong` at `:844` | PASS |
| 10 | nvrhi claim A | read `vulkan-compute.cpp:145` | `commitBarriers();` inside `setComputeState` | exact | PASS |
| 11 | nvrhi claim B | read `vulkan-compute.cpp:166-173` | `dispatch` body has no barrier call | body is `assert` + `updateComputeVolatileBuffers()` + `cmdBuf.dispatch()`; `:173` closes it | PASS |
| 12 | Generation flushes first | read `FReSTIRPass.cpp:400` vs `:481`/`:489` | `setComputeState` precedes temporal `createBindingSet` | confirmed | PASS |
| 13 | Contrast case is a different hazard | read the `commitBarriers()` before `ReBLURPass.Dispatch` | its comment cites the intra-`setComputeState` bind-before-barrier hazard | confirmed | PASS |
| 14 | Deadness unchanged by this cycle | `search_files pattern="AccumInput"` | exactly 2 assignments | **4 hits**: `:1123` decl+ternary, `:1180` reassign, `:1190`/`:1203` uses → 2 assignments | PASS |
| 15 | Half-res plumbing untouched | `search_files pattern="HalfResWidth"` | 17, only `:882` in this block | **17**; `:793`/`:901`/`:988`/`:1069`/`:1099`/`:1592`/`:2838` + constants all intact | PASS |
| 16 | No shader edited | edited-file list for this cycle | 1 `.cpp` only | only `TestReSTIR_GI_Temporal.cpp` | PASS |
| 17 | Cornell untouched | not in this cycle's file list | — | — | PASS |

**17/17 PASS.**

## Discriminators vs. weak rows

Rows 6, 9, 11 and 14 are genuine discriminators — each could independently have
failed and forced a FIX:

- **Row 6** is the one v189 failed at first draft (it wrote `:1148`/`:1570`
  references that were stale the moment they were written). It passes here, and
  file-wide rather than hunk-local.
- **Row 9** guards against the correction being ambiguous: the false mechanism
  is still present *as a quotation*, and the row confirms it is immediately
  followed by its refutation rather than left standing.
- **Row 11** is the whole cycle's premise. If `dispatch` had contained a barrier
  call, the new comment would be as wrong as the one it replaced.
- **Row 14** confirms this cycle did not disturb v189's deadness argument.

Rows 1-5 and 15-17 are individually weak (they assert absence of collateral
damage) but are kept separate rather than collapsed, because the alternation
form that would merge them returns 0 hits on this runspace and would read as a
false failure.

## Not run

No build, no execution, no shader compile, no image. `terminal` denied
(`tirith:unknown`). Rows 10-13 are source reads, not runtime observations.
