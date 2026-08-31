# Pending Commit v192

- plan: docs/PENDING_PLAN_v192.md
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
- source: no bundle — direct edit
- target: (working tree only — no commit, per job instruction)
- task: card E — the resolve pass is dispatched and scaled from the resizable
  swapchain while every resource it touches is fixed-size. Three functional lines.
- verify: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild`
- skip_impl_review: no
- produces_test_files: no
- impler: agent_3_impler (tick-538)

## The change

Three functional lines, all inside the Phase-D resolve block:

```cpp
-            RC.RcpFullW = 1.0f / static_cast<float>(FB.width);
-            RC.RcpFullH = 1.0f / static_cast<float>(FB.height);
+            RC.RcpFullW = 1.0f / static_cast<float>(WIDTH);
+            RC.RcpFullH = 1.0f / static_cast<float>(HEIGHT);

-                CommandList->dispatch((FB.width + 7) / 8, (FB.height + 7) / 8, 1);
+                CommandList->dispatch((WIDTH + 7) / 8, (HEIGHT + 7) / 8, 1);
```

Plus 17 comment lines at the constants site and 1 at the dispatch. Net
**+3 / -3 functional, +18 comment**.

**No shader was touched.** `Resolve_cs.hlsl` is byte-unchanged, per the plan
gate's finding that `int2 fp = hp * 2 + 1` encodes the fixed half-to-full
footprint relationship (both extents descend from `WIDTH`/`HEIGHT`) rather than a
swapchain ratio. The comment records that explicitly, as the plan-criticer
required, so the next reader does not re-open card E's question.

At the default extent this changes nothing: `1/800` either way, and the grid is
identical.

## Plan Deviations

None. The plan specified three substitutions, banned the shader edit, banned
line-number references in the comment, and required the shader note. All four
honored.

Specifically on the third: v191's impler wrote stale `:NNNN` references and had
to correct them mid-cycle. This cycle used **symbol names only**
(`CreateGBufferTextures`, `OutputTexture`, `FullResGIRaw`, `Resolve_cs.hlsl`) and
one back-reference phrased as "See the note above." Verified below.

## Static verification performed (no shell — file-only)

Every row re-executed this turn. **Per the v191 audit's rule, no count is quoted
from a previous marker, and every zero is confirmed against a positive control.**

| # | Claim | Query | Result | Verdict |
|---|---|---|---|---|
| 1 | Constants use the fixed extent | `RcpFullW` | 2 hits: struct field `:1101`, assignment `:1126` `1.0f / static_cast<float>(WIDTH)` | PASS |
| 2 | Dispatch uses the fixed extent | `dispatch((WIDTH` | 1 hit `:1156` `dispatch((WIDTH + 7) / 8, (HEIGHT + 7) / 8, 1)` | PASS |
| 3 | Resolve block free of swapchain extent | `FB\.width` | 15 hits; the surviving code sites are `:754/756` resize check, `:764` view constants, `:773` raster, `:1182/1184/1203` ReBLUR, `:1238/1255` accumulate, `:1269` blit — **none between `:1104` and `:1160`** (the resolve block); `:863/1024/1029/1085/1113` are comment text | PASS |
| 4 | Shader untouched | `hp \* 2 \+ 1` in `Resolve_cs.hlsl` | 1 hit `:60`, unchanged; the file is absent from `files:` above | PASS |
| 5 | Half-res operands still correct | `RcpHalfW` | untouched, still `1.0f / static_cast<float>(HalfResWidth)` | PASS |
| 6 | Outputs really are fixed-size | `DispatchResolve` | 3 hits: definition `:1132`, calls `:1159` `(OutputTexture, FullResGIRaw)` and `:1160` `(SpatialRadiance, FullResSpatial)`; both outputs created via `CreateTexture2D(NvrhiDevice, W, H, ...)` in `CreateGBufferTextures` | PASS |
| 7 | `HEIGHT` unshadowed at the site | `HEIGHT` | 8 hits; sole declaration `:107`; locals `H = HEIGHT` at `:1559/:1685/:1778`, all in other functions; none in `Render()` | PASS |
| 8 | No new stale cross-reference | `// .*:[0-9][0-9][0-9]` | 5 hits (`:894`, `:990`, `:993`, `:994`, `:2323`) — the same five as before this cycle, all pointing into *other* files | PASS |
| 9 | v191's fix undisturbed | `WIDTH / std` | 2 hits `:1039`, `:1087` — both v191 sites intact | PASS |

**Positive controls for the zero-adjacent rows**: row 3's claim is an absence
within a line range, and `FB\.width` alone returns 15 in this file, so the query
shape demonstrably matches text here — the absence in `:1104-1160` is a source
fact, not a tool artifact.

## NET-NEW while verifying: a SEVENTH instance, in the accumulate pass

Row 3 forced me to justify every surviving `FB.width`, and two of them do not
survive justification.

`:1238-1239` `AccC.Width = FB.width; AccC.Height = FB.height;` and `:1255`
`dispatch((FB.width + 7) / 8, (FB.height + 7) / 8, 1)` — but the pass's two UAV
outputs, `AccumTexture` (u0) and `DisplayTexture` (u1), bound at `:1246-1247`,
are created at `:1658-1661` via `CreateTexture2D(NvrhiDevice, W, H, ...)` inside
`CreateGBufferTextures`. **Fixed-size.** Its SRV input `AccumInput` is
`FullResGIRaw`/`FullResSpatial`, also fixed. Identical shape to card E.

`DisplayTexture` is the texture the `display` dump comes from — the one gate 6
and `validate_restir_gi.py` both read. So this instance sits directly on the
acceptance path, unlike v189/v191/v192.

**Not bundled.** Different pass, and bundling would make row 3 of this very table
unverifiable. Opened as card F.

## What is NOT established

Not built, not compiled, not run, not validated, no image viewed. `terminal` is
denied. Argued from source alone.

**Falsifiable prediction:** at 800x600 this patch must produce byte-identical
output to a v191 build. If it does not, either the patch is wrong or `FB.width`
is not 800 at startup on that machine — which would itself be the finding, and
would simultaneously mean the pre-patch code was already miscomputing.
