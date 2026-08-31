# Pending Tests v204

- commit: docs/PENDING_COMMIT_v204.md
- tester: agent_5_tester (tick-550)
- mode: FILE-ONLY. Nothing was built, compiled, run, rendered or viewed.

Every row re-derived by me. Per tick-526 no `|` alternation; per v199 no
`path`-at-a-file for a load-bearing negative; per v203 row 16 every diff read.
Every zero is paired with a same-shape positive control.

| # | Claim | Query / method | Result | Verdict |
|---|---|---|---|---|
| 1 | Guides are full-res | read `TestReSTIR_GI_Temporal.cpp:1655-1657` | `LinearDepthTexture` created from `W, H` | PASS |
| 2 | Dispatch is half-res | read `:1671-1673` | `HalfResWidth = HalfW = W / 2` | PASS |
| 3 | Both meet at one call site | read `:880-897` | full-res guides + half-res output/input in one `FDesc` | PASS |
| 4 | Shader bound by inverted TexelSize | read shader `:74-77` | `outputSize = 1/TexelSize`, from `OutputWidth` | PASS |
| 5 | All depth loads remapped | `t_Depth.Load` | 2 hits, both `GB(...)` | PASS |
| 6 | All normal loads remapped | `t_Normal.Load` | 2 hits, both `GB(...)` | PASS |
| 7 | Input loads NOT remapped | `t_Input.Load` | 2 hits, both raw `pixelCoord` | PASS |
| 8 | Helper present once | `GuideScale` in primary data dir | 3 hits: decl `:21`, comment `:33`, use `:37` | PASS |
| 9 | C++ writes the slot once | `GuideScale` in PostProcess dir | 4 hits, one assignment `:174`, one derive `:179`, one write `:181` | PASS |
| 10 | No cbuffer overflow | read `:156` | `float ConstantsData[64]`, write at index 5 | PASS |
| 11 | Dual-copy checked | `BilateralDenoise_cs.hlsl` (files) | **2 files** — both inspected | PASS |
| 12 | Control shader `main` byte-unchanged | `t_Normal.Load` in control dir | 2 hits, both **raw** `pixelCoord` — no `GB()` introduced | PASS |
| 13 | Control has no `GB()` helper | `GB(` in control dir | 1 hit, and it is in `ReSTIR_Spatial_cs.hlsl` (unrelated pre-existing comment), **0 in `BilateralDenoise_cs.hlsl`** | PASS |
| 14 | Control ratio is exactly 1 | `GBufferWidth =` in control | 2 assignments, `:521` and `:1166`, both framebuffer width = the dispatch quantity at `:1483` | PASS |
| 15 | `FReBLURPass` clean (v202 inv.) | read both `ReBLUR_cs.hlsl` headers | identical b0/t0-t3/s0/s1/u0 in both, matching the 8-item layout `:102-109` | PASS |
| 16 | `FReBLURPass` clean (v183 inv.) | read `FReBLURPass.cpp:214-223` + header `:46-51` | cbuffer tail agrees across C++ struct, marshaller and both copies | PASS |
| 17 | Domain derived, not recalled | `FReSTIRPass` / `FBilateralDenoisePass` / `FGIPass` over Test dir | `FGIPass` → 0 hits in Test dir (not shared, correctly excluded); others 2 consumers each | PASS |

## Row 13 is the one I improved

The impler asserted "the control's copy has no `GB()`" — a negative. A bare
`GB(` query over the control directory returns **1 hit**, which taken at face
value refutes the claim. Reading it shows the hit is in `ReSTIR_Spatial_cs.hlsl`
line 31, inside a pre-existing comment warning against adding a `GB()` there —
unrelated to this cycle, and present before it.

So the raw count is a **false positive** for this claim, in the same family as
v192's two false zeros of opposite polarity. The sound form of the row is
per-file, not per-directory. Recorded because a future sweep that greps `GB(`
across the control to check "was the control perturbed" would get a hit and
draw the wrong conclusion.

## Row 7 is the one that carries the cycle

Rows 5/6 confirm the fix was applied; row 7 confirms it was **not over-applied**.
Those are different claims and only row 7 is at risk, because the natural
mechanical edit routes every `Load` through the new helper. Doing so to
`t_Input` would reintroduce the exact out-of-bounds half-res read that v189
fixed at this call site — turning a correctness fix into a regression of a
previously-fixed bug in the same function.

## NOT established

That anything compiles, links, runs, renders or validates. No row above may be
cited as evidence that an acceptance gate passed. `Build.sh` and every other
shell invocation are refused by tirith at this runspace.
