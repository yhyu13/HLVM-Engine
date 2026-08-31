# Pending Tests v210

- commit: docs/PENDING_COMMIT_v210.md
- tester: agent_5_tester (tick-556)
- mode: **file-only** — terminal denied at the tool boundary this tick
  (a bare `true` returns `pending_approval / tirith:unknown / exit_code -1`),
  so nothing here is an execution result. Every row is a static check and is
  labelled as one.

## Verifier rows

| # | Check | Method | Result |
|---|---|---|---|
| 1 | Patch present at `FTemporalDesc` | `EXTENT CONTRACT` on the PostProcess header dir → **1**, in `FReSTIRPass.h` | PASS |
| 2 | Zero in row 1 is real, not a false zero | same query, same dir, returns 0 for the other 13 headers and 1 for the target — a same-shape positive inside the same call | PASS |
| 3 | Diff was additive only | returned diff read: two hunks, zero `-` lines | PASS |
| 4 | Nothing displaced | 169 → 191 lines = +22 = comment lines added | PASS |
| 5 | Struct members intact | `nvrhi::TextureHandle` → **26** in file; all 14 `FTemporalDesc` + 8 `FSpatialDesc` members present in original order | PASS |
| 6 | File structure intact | class closes `:190`, namespace `:191`, 191 total | PASS |
| 7 | Comment claim: callee never `getDesc()`s a guide | `getDesc` → 3 in `FReSTIRPass.cpp`, all `OutReservoir0`/`OutRadiance` | PASS |
| 8 | Comment claim: one scale, four guides | `GB(` → 5 in primary shader (def `:78` + `:135`/`:178`/`:179`/`:180`) | PASS |
| 9 | Comment claim: `FReBLURPass` has no scale field | `GuideScale` → 0 in `.cpp`, controlled by 1 in `.h` | PASS |
| 10 | Both consumers still set the field | primary `:1061`/`:1109`; control `:1592`/`:1645` | PASS |

## Row 11 — added unprompted, and it is the one that could have mattered

Nobody specified it. The only way a comment-only edit to this header could do
real harm is by disturbing the **v203 near-miss restoration** — that cycle's
third `patch` deleted three live binding items from `SpatialLayout` because an
`old_string` anchored on a comment matched into a braced initialiser, and this
cycle edited comments adjacent to two braced member lists in the same class's
header.

`BindingSetItem::` in `FReSTIRPass.cpp` → **29 hits**, byte-identical to the
count v208 derived independently before this cycle existed. The initialiser
lists are untouched. **PASS.**

This is the row that makes the "0 functional lines" banner falsifiable rather
than merely asserted — the failure v203 proved is possible.

## Row 12 — the dual-copy axis the reviewer opened

`GB(` → **0** in `TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl`, controlled
by `Load` → **9** in the same file under the same query shape. Confirmed no
`search_timeout` and no `error` field on that zero (audit rows 18 / v205).
The copies are **correctly different**: the control is not half-res and sets
`GBufferScale = 1.0f`, for which `GB()` would be the identity map; it keeps
the field declared to hold the cbuffer tail aligned (v184/v186/v200). **PASS,
recorded as a correct divergence, NOT carded as a defect.**

## What these rows do NOT establish

That anything compiles, links, runs, renders, or validates. **0 of the job's
7 acceptance gates are touched by this cycle**, and the cycle does not claim
otherwise. No `/tmp/hermes-verify-*` harness was written: it could not be
executed, would yield zero evidence, and would strand another uncleanable
orphan beside the ones this lineage has already left.
