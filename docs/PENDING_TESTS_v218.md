# Pending Tests v218

- commit: docs/PENDING_COMMIT_v218.md
- tester: agent_5_tester (tick-566)
- timestamp: 2026-08-21
- mode: **file-only.** `terminal` refused this tick (`pending_approval` / `tirith:unknown` /
  `exit_code -1`), including the degenerate command `true`, so the block is command-independent. No
  build, no run, no image viewed. Nothing below is a runtime result.

## Method

Every row re-derived this turn, not read from the commit marker. Per the v217 rule each load-bearing
**zero** is paired with a same-shape, same-scope **positive control**; `path` is a single file or
`Engine/Source/Runtime`, never the project root (v217 false-partial-result class); no `file_glob`
(v217), no `|` alternation (tick-526), no unescaped ERE metacharacters (v208).

## Rows

| # | Row | Scope | Result | Control | Verdict |
|---|---|---|---|---|---|
| 1 | `DummyDirectionTexture` | Runtime | **0** | `DummyDebugStatsTexture` → **5** same scope/shape | PASS |
| 2 | `DummyDebugStatsTexture` present at `FGIPass.h:139` + 4 uses in `.cpp` | Runtime | 5 | — | PASS |
| 3 | `waitForIdle` in `FGIPass.cpp` | file | **3** (`:177` comment, `:197`, `:441`) | non-zero | PASS |
| 4 | `executeCommandList` in `Private/Renderer/GI` | dir | **2** (`:196`, `:440`) | non-zero | PASS |
| 5 | Neither #3 nor #4 lands in `DispatchRays` (v214 intent) | file | confirmed by enclosing scope | — | PASS |
| 6 | `RenderGBuffer(` def + call, both zero-arg (v197 arity) | file | def `:2173`, call `:795` | — | PASS |
| 7 | Temporal cbuffer tail = 3 plain scalars, no array (v184) | 4-way | `FReSTIRPass.h:57-59`, marshaller `:544-546`, HLSL `:24-30` | — | PASS |
| 8 | Spatial cbuffer tail = `GBufferScale`,`Pad`, no array | 4-way | `.h:71-72`, marshaller `:637`, both HLSL copies | — | PASS |
| 9 | `GuideScale` at float **slot 5** in C++ | file | `FBilateralDenoisePass.cpp:215` `ConstantsData[5]` | — | PASS |
| 10 | Slot 5 is a `float` in all **three** HLSL copies | 3 files | shared `:21`, RGI `:21`, Cornell `:26` | — | PASS |
| 11 | `GuideScale_Unused` unique to the Cornell copy | Runtime | **1** | `GuideScale` → 16 | PASS |
| 12 | Cornell copy indexes guides RAW (no `GB()`), by design | file | `t_Depth` `:71`/`:102` raw | RGI copy `:82`/`:113` via `GB()` | PASS |
| 13 | 3 copies of `BilateralDenoise_cs.hlsl` exist | Runtime | **3** | — | PASS |
| 14 | Each copy compiled by its own cfg | files | `Shader/ShaderMake.cfg:5`, `..._Data/ShaderMake.cfg:3` | — | PASS |
| 15 | Consumer selects copy via `Initialize` arg | file | `:44` store, `:47-48` combine | — | PASS |
| 16 | **`SetShaderDataDir` has zero call sites** | Runtime | **4 hits, 0 calls** (def, decl, 2 comments) | `ShaderDataDir` → 50 same scope | PASS |
| 17 | Override reaches **only** Blit shaders | file | `:44`→`:69-71`→`:319`→`:322`→`:210`/`:236` | — | PASS |
| 18 | `FBilateralDenoisePass.cpp` never mentions `FCommonRenderPasses` | file | **0** | `nvrhi` → non-zero, same file | PASS |
| 19 | `g_ShaderDataDirOverride` fully enumerated | file | **4** (decl, 2 reads, 1 write) | — | PASS |
| 20 | v214 marker predicts `waitForIdle` → 1 at `:415` | file | actual **2 code hits**, at `:197`/`:441` | — | **MISMATCH — finding 2 confirmed** |
| 21 | Zero source files modified this cycle | — | no `patch`/`write_file` touched any source path | — | PASS |

## Row 20 is the one with consequences

It is the only row that fails, and it fails **against a closed cycle's marker, not against the tree**.
The tree is correct: v214 relocated the stall out of the per-frame path into `Initialize`, which is
what it set out to do. The marker's `verify:` line describes an outcome (call deleted) that the patch
never produced (call moved). An operator running that line on a healthy tree sees a mismatch and
concludes the patch is missing — a **false failure**, worse than a false pass per v192, and pointed at
the one file whose per-frame path this lineage has already broken and repaired once.

## Row 16-18 note (the control that matters)

Row 18's zero is the kind this lineage has been burned by, so it carries a same-file positive. Row 16's
"4 hits, 0 calls" is deliberately reported as a **classification, not a count** — the count alone reads
as "the symbol is used", and only reading each of the four shows none is an invocation.

## Not established

Nothing here shows the code compiles, links, runs, renders or validates. Rows 7-10 reduce the risk that
the operator's first build produces a *silently wrong* image; they cannot show it produces a right one.
