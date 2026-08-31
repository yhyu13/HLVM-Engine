# Pending Tests v200

- commit: docs/PENDING_COMMIT_v200.md
- tester: agent_5_tester (tick-546)
- timestamp: 2026-08-30
- test files produced: **none** (the commit is an audit; the "tests" are
  independent re-derivations of each load-bearing claim)

## Method

File-only verifier. Standing rules honoured: no `|` alternation (tick-526);
no `output_mode=count` with `path` at a directory (v199); every load-bearing
**zero** paired with a positive control of the same query shape; counts
re-derived from source, never quoted from another marker.

## Rows

| # | Claim | Query | Result | Verdict |
|---|---|---|---|---|
| 1 | `RenderGBuffer` arity consistent | `RenderGBuffer(` over `Runtime` | 2 hits: def `:2173`, call `:795`, both zero-arg | PASS |
| 2 | row 1's "2" is not a false low | `RenderGBuffer` unparenthesised, same path | 13 hits | PASS (positive control) |
| 3 | C++ temporal tail order | read `FReSTIRPass.h:51-59` | SceneYaw, PrevSceneYaw, NearPlane, FarPlane, GBufferScale | PASS |
| 4 | marshaller temporal tail order | read `FReSTIRPass.cpp:441-456` | identical order | PASS |
| 5 | primary HLSL temporal tail | read `…GI_Temporal_Data/…:33-42` | identical order | PASS |
| 6 | control HLSL temporal tail | read `TestCornellBoxGI_Data/…:36-40` | identical order | PASS |
| 7 | no live array in temporal tail | `Pad\[` in `FReSTIRPass.h` | 3 hits, **all in comments** (`:54`, `:57`, `:58` trailing `// was Pad[0]`) | PASS |
| 8 | same, primary HLSL | `Pad` in primary temporal HLSL | 4 hits, all comments; declarations at `:40`/`:41` are scalars | PASS |
| 9 | spatial tail, all four | reads of `.h:64-72`, `.cpp:536-547`, both HLSL | agree; marshaller omits `Pad` only | PASS |
| 10 | omitting `Pad` is safe | read `FReSTIRPass.cpp:533` | `memset(ConstantsData, 0, sizeof(...))` precedes marshalling | PASS |
| 11 | no cbuffer overflow | recount `:428-456` | 32 matrix + 14 scalars = **46 of 64** | PASS |
| 12 | GBufferScale assigned, primary | `GBufferScale` in `TestReSTIR_GI_Temporal.cpp` | 3 hits: `:1056` comment, `:1061` temporal assign, `:1109` spatial assign | PASS |
| 13 | GBufferScale assigned, control | `GBufferScale` in `TestCornellBoxGI.cpp` | 4 hits: 2 comments, `:1592` + `:1645` assigns `1.0f` | PASS |
| 14 | both structs `{}`-initialized | `FReSTIRTemporalConstants` / `FReSTIRSpatialConstants` tree-wide | 10 hits each; call sites `TC{}`, `SC{}`, `TempConstants{}`, `SpatConstants{}` | PASS |
| 15 | dual-copy sync, primary | `gbPixel` in `…_Data/GIPathTracing.hlsl` | 12 hits | PASS |
| 16 | dual-copy sync, source of truth | `gbPixel` in `Private/…/GI/GIPathTracing.hlsl` | 12 hits, **identical line numbers** | PASS |
| 17 | `WIDTH`/`HEIGHT` are compile-time constants | `WIDTH =` / `HEIGHT =` in primary | 1 hit each: `:106` `= 800`, `:107` `= 600`, both `static const uint32_t` | PASS |
| 18 | v197 log fix landed | read `:2434-2435` | logs `WIDTH, HEIGHT`; 4 placeholders, 4 args | PASS |

**18/18 PASS.**

## Row 11 — I contradict the impl marker, and agree with the reviewer

The commit says 13 scalars / 45 of 64. I counted **14 scalars / 46 of 64**
independently, before reading the impl review, and then found the reviewer had
reached the same 46. Two independent recounts against the marker's 45.

The conclusion (no overflow) is unaffected either way, which is exactly why this
row matters: a harmless miscount is the kind that propagates, because nothing
downstream fails to flag it.

## Row 7 — the row most likely to have produced a false pass

Rows 7 and 8 are the v184 guard: *is there a live array in the cbuffer tail?*
The natural query is `Pad[`, and it returns hits — which could be read as "an
array is present," a false FAILURE of the v192 kind. Every hit is a **comment**
(`// was Pad[0]`, and a prose line about the `Pad[2]` that used to be there).

So this row cannot be settled by a count at all; it required reading each hit.
Recorded because a future tick running `Pad\[` and seeing 3 hits could conclude
the opposite of the truth in either direction.

## What was NOT tested

Nothing was compiled, built, run, shader-compiled, validated, or viewed. Every
row above is a static source determination. **Rows 1-18 passing does not mean
the target builds** — it means the two defect classes this lineage has
demonstrated are absent from the chain.
