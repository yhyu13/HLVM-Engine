# Pending Tests v195

- commit: docs/PENDING_COMMIT_v195.md
- tester: agent_5_tester (tick-541)
- mode: **file-only** — `terminal` denied at three distinct invocation shapes
  this tick (compound command, bare `/bin/true`, `./Build.sh`), all
  `pending_approval / tirith:unknown / exit_code -1`. The denial is
  categorical, so no row below executes anything.
- query discipline: **no `|` alternation in any pattern** (tick-526 rule), `path`
  always at a directory, plain substrings only, every zero controlled.

## Rows

| # | Row | Query / method | Expected | Result |
|---|---|---|---|---|
| 1 | Call site substituted | `UpdateViewConstants(WIDTH, HEIGHT)` | 1 | **1** (`:787`) PASS |
| 2 | Only one call site exists | `UpdateViewConstants(` | 2 (call + defn) | **2** (`:787`, `:2427`) PASS |
| 3 | Old call form gone | `UpdateViewConstants(FB.width` | 0 | **0** PASS (controlled by row 1) |
| 4 | Viewport substituted | `Vp(0.f, float(WIDTH), 0.f, float(HEIGHT)` | 1 | **1** (`:2356`) PASS |
| 5 | Old viewport form gone | `float(LastWidth)` | 0 | **0** PASS (controlled by row 6) |
| 6 | **Zero-control for row 5** | `float(WIDTH)` | ≥1 | **1** (`:2356`) PASS |
| 7 | Resize detection preserved | `LastWidth = FB.width` | 1 | **1** (`:756`) PASS |
| 8 | Resize comparison preserved | `FB.width != LastWidth` | 1 | **1** (`:754`) PASS |
| 9 | `LastWidth` residual set | `LastWidth` | 4 (cmp, assign, log, decl) | **4** PASS — viewport use gone |
| 10 | `LastHeight` residual set | `LastHeight` | 4 | **4** PASS |
| 11 | Complete `FB.width` partition | `FB.width` | 13, all classified | **13** PASS (9 comment, 2 resize, 1 inert callee, 1 blit) |
| 12 | Blit untouched | `&BindingCache, FB.width, FB.height, BlitParams` | 1 | **1** (`:1327`) PASS |
| 13 | `WIDTH` single definition | `WIDTH =` | 1 | **1** (`:106`) PASS |
| 14 | `HEIGHT` single definition | `HEIGHT =` | 1 | **1** (`:107`) PASS |
| 15 | Shadowing decls enumerated | `const uint32_t W = WIDTH` | 4 | **4** (1617, 1743, 1836, + `:2344` comment) PASS |
| 16 | Shadowing **containment** check | ranges: `Render()` 746→, patch `:787`; `RenderGBuffer` `:2165`→, patch `:2356`; nearest decl `:1836` | none inside either | PASS |
| 17 | Shader copy A unchanged | `gbScale` in `TestReSTIR_GI_Temporal_Data` | 3 | **3** (498, 499, 757 comment) PASS |
| 18 | Shader copy B unchanged | `gbScale` in `Private/Renderer/Shader/GI` | 3 | **3** (498, 499, 757) PASS — **both copies identical in this region**, so the v182 dual-copy hazard is not engaged |
| 19 | Defect model: numerator | `RenderTargetSize` in `Runtime` | present at `GIPathTracing.hlsl:76,498,525,526` | PASS |
| 20 | Defect model: denominator fixed | `Desc.OutputWidth = HalfResWidth` `:793`; `HalfResWidth = HalfW` `:1651`; `HalfW = W / 2` `:1649`; `W = WIDTH` `:1617` | chain fixed | PASS |
| 21 | Dispatch consumes it | `RTPipeline.DispatchRays(CmdList, Desc.OutputWidth` in `FGIPass.cpp` | 1 (`:741`) | PASS |
| 22 | Aspect answer: blit stretches | read `BlitShader.hlsl:15-29` + `FCommonRenderPasses.cpp:389-391` | no letterbox term | PASS |
| 23 | Marker comments present | `v195` | 2 | **2** (`:764`, `:2342`) PASS |
| 24 | No shader file edited | `v195` in `TestReSTIR_GI_Temporal_Data` | 0 | **0** PASS (controlled by row 23) |

**24/24 PASS, file-only.**

## Rows that test the *model*, not the diff

Rows 19-22 exist because, per the v194 audit's lesson, a suite that only
confirms the substitution is present cannot detect a correct patch applied for
the wrong reason. Rows 19-21 walk the `gbScale` numerator/denominator chain
independently of the plan's prose; row 22 answers the card's design question
from the presentation stage rather than from preference.

## What these rows CANNOT establish

- that the file compiles (`./Build.sh` denied)
- that the target links
- that any pixel, dump, `M mean` or validator output changes
- gate 6 — this runspace exposes **no vision/image tool at all**, so no PNG can
  be inspected here regardless of the shell block

**No build, no run, no image. This is neither suite-green nor ad-hoc-green.**
