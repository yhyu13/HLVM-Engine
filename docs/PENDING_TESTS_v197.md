# Pending Tests v197

- commit: docs/PENDING_COMMIT_v197.md
- mode: **file-only.** `terminal` probed this tick and refused
  (`pending_approval`, `tirith:unknown`, `exit_code -1`). Nothing here was
  compiled, built, run, or viewed. These rows verify **that the intended edit is
  present and complete**, which is a strictly weaker claim than "it works."
- rows: 14

Every zero below is paired with a same-shape positive. No `|` alternation
(tick-526). No query pasted from a wrapping line (v196). No query encoding an
assumption about a declaration form without a widening control (v197, new).

| # | Query | Path | Expect | Got | Verdict |
|---|---|---|---|---|---|
| 1 | `RenderGBuffer()` | target | 2 (0 before) | **2** — `:804` call, `:2179` def | PASS |
| 2 | `RenderGBuffer(FB.width, FB.height)` | target | 0 (1 before) | **0** | PASS |
| 3 | `void RenderGBuffer(uint32_t` | target | 0 (1 before) | **0** | PASS |
| 4 | `RenderGBuffer(` | Runtime tree | 2 | **2** — def + call only | PASS |
| 5 | `MeshCount, WIDTH, HEIGHT` | target | 1 (0 before) | **1** — `:2446` | PASS |
| 6 | `MeshCount, LastWidth, LastHeight` | target | 0 (1 before) | **0** | PASS |
| 7 | `uint32_t WIDTH` | target | 1 | **1** — `:106` `static const` | PASS |
| 8 | `LastHeight` | target | ≥3 live | **8**; live: `:754`, `:757`, `:3015` decl | PASS |
| 9 | `FB.width` | target | 3 live | **14 total, 3 live**: `:754`, `:756`, `:1335` | PASS |
| 10 | `nvrhi::Viewport Vp` | target | 1, `WIDTH`/`HEIGHT` | **1** — `:2353` unchanged | PASS |
| 11 | `viewport {}x{}` | target | 1 | **1** — `:2446` fmt string intact | PASS |
| 12 | `v197` | target | 3 | **3** — `:793`, `:2173`, `:2435` | PASS |
| 13 | `v197` | `TestPathTraceGI.cpp` | 0 | **0** | PASS |
| 14 | `v197` | `GIPathTracing.hlsl` | 0 | **0** | PASS |

## Notes on the rows that carry weight

**Rows 1/2/3 are the cycle.** Row 1's `2` is what controls rows 2 and 3: the same
identifier returns non-zero in the same file with the same tool, so their zeros
are real absences and not malformed queries. Before the patch row 1 was `0` and
rows 2/3 were `1` each — the polarity inverted exactly as v197.2 predicted.

**Row 4 is the compile-safety row.** An arity change is only safe if the caller
set is closed. Two hits tree-wide, both accounted for. If a third existed, the
first build of the v183-v197 chain would fail on it.

**Row 7 exists because the impler's first attempt at it returned a false zero.**
`constexpr uint32_t WIDTH` → 0; the declaration is `static const`, not
`constexpr`. The row as written here uses the metacharacter-free fragment common
to both forms. Recording it because the zero *looked* like "WIDTH is not a
compile-time constant and may not be in scope" — a finding — when it was only a
wrong guess about syntax.

**Rows 8 and 9 are the no-collateral-damage rows.** Row 8 confirms
`LastWidth`/`LastHeight` still have live uses after this cycle removed one, so
they do not become unused variables. Row 9 confirms the three deliberate
`FB.width` sites survive untouched and that the fourth — the one card K was
about — is gone. **This is the first time in the lineage that the primary
target's live `FB.width` set contains only intentional sites.**

**Rows 13/14 are control rows**, and their zeros are controlled by row 12's `3`
and by the four `v197` marker files in `docs/`. They establish that this cycle
touched neither the known-good control nor either shader copy — so the v182
dual-copy hazard is not engaged and v196's protection of the control holds.

## What these rows do NOT establish

That the file compiles. That it links. That it runs, renders, or validates.
**The v183-v197 chain has never been built.** Row 4 is the strongest available
file-only proxy for compile-safety of the arity change, and it is a proxy.
