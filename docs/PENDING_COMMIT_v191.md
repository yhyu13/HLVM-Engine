# Pending Commit v191

- plan: docs/PENDING_PLAN_v191.md
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
- source: no bundle — direct edit
- target: (working tree only — no commit, per job instruction)
- task: GBufferScale must be derived from the fixed GBuffer width (WIDTH), not the
  resizable swapchain extent (FB.width). Two functional lines.
- verify: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild`
- skip_impl_review: no
- produces_test_files: no
- impler: agent_3_impler (tick-538)

## The change

Two functional lines, both in `Render()`:

```cpp
-            TC.GBufferScale = static_cast<float>(FB.width / std::max(HalfResWidth, 1u));
+            TC.GBufferScale = static_cast<float>(WIDTH    / std::max(HalfResWidth, 1u));

-            SC.GBufferScale = static_cast<float>(FB.width / std::max(HalfResWidth, 1u));
+            SC.GBufferScale = static_cast<float>(WIDTH    / std::max(HalfResWidth, 1u));
```

Plus 18 comment lines at the temporal site stating the invariant, and 2 at the
spatial site pointing to it. Net **+2 / -2 functional, +20 comment**.

At the startup extent nothing changes: `800 / 400 == 2` either way. The patch
removes a latent silent-failure mode, and is deliberately a no-op on the run the
operator is about to do. That is the point — it must not perturb the
v183/v184/v185 chain awaiting its single run.

## Plan Deviations

**One, and it was self-inflicted — recorded because it is the exact defect class
v189 and v190 caught, and I walked straight into it.**

The plan asked for a comment stating the invariant. My first draft cited source
positions by line number: `(:1523)`, `(:1292)`, `(:1578)` in the temporal comment
and `see the comment at :1023` in the spatial one. **All four went stale the
moment my own 18-line insertion shifted the file.** Verified, not assumed:
`search_files pattern="GBufferScale"` after the insert showed the temporal site
had moved to `:1039`, so the spatial comment's `:1023` pointed 16 lines above its
target; `const uint32_t W = WIDTH` re-queried to `:1541`, not `:1523`;
`CreateGBufferTextures`'s half-res block to `:1596`, not `:1578`.

v190's audit established `// .*:[0-9][0-9]+` → **0 hits file-wide** in this file
and called that "stronger than v189 achieved." I would have regressed it in the
first cycle after it was won.

Fix: all four line-number references replaced with **symbol names**
(`CreateGBufferTextures`, `BackBufferResizing`, `HalfResWidth`) and one grep
anchor (`search: "v191: the numerator"`). Symbols do not rot when the file
shifts.

Residual: `search_files pattern="// .*:[0-9][0-9][0-9]"` → 5 hits remain at
`:894`, `:990`, `:993`, `:994`, `:2305`. **All five pre-date this cycle** (they
are v182/v185-era references into `FReSTIRPass.cpp`, `ReSTIR_Temporal_cs.hlsl`
and `FGIPass.cpp` — *other* files, which my edit cannot shift). I did not add
to that set and I did not clean it up either; out of scope, and touching them
would inflate a two-line patch.

## Static verification performed (no shell — file-only)

| # | Check | Query | Result |
|---|---|---|---|
| 1 | Both sites now use `WIDTH` | `GBufferScale` in this file | 9 hits; `:1039` and `:1087` both read `static_cast<float>(WIDTH / std::max(HalfResWidth, 1u))` |
| 2 | No `FB.width` left in either ReSTIR constants block | `FB\.width` | present only at `:754/756/764/773` (resize check, view constants, raster) — none inside the temporal or spatial blocks |
| 3 | `WIDTH` is not shadowed at the patch site | `WIDTH` | 40 hits; sole declaration `:106`; every local `W = WIDTH` is in another function (`CreateGBufferTextures`, `FillGBufferHardcoded`, and one at `:1760`) — none in `Render()` |
| 4 | Numerator matches the textures actually indexed | `const uint32_t W = WIDTH` | `:1541` opens `CreateGBufferTextures`, which creates `GBufferNormal` and `LinearDepthTexture` from that `W` |
| 5 | Denominator untouched | `HalfResWidth` | 17 hits, unchanged count; the guard `std::max(HalfResWidth, 1u)` survives at both sites |
| 6 | No new stale cross-reference | `// .*:[0-9][0-9][0-9]` | 5 hits, all pre-existing and all pointing into *other* files |
| 7 | Cornell untouched | `GBufferScale` tree-wide | 21 hits; `TestCornellBoxGI.cpp:1592/1645` still `1.0f` literals |
| 8 | Shader side unchanged | `GB(` helper | `ReSTIR_Spatial_cs.hlsl:52-56` and `ReSTIR_Temporal_cs.hlsl:80` untouched — no shader file was edited, so the v182 "patched a copy nothing compiles" trap is not engaged |

## What is NOT established

Not built, not compiled, not run, not validated, no image viewed. `terminal` is
denied. The patch is argued from source alone.

**Falsifiable prediction for the operator:** at the default 800x600 extent this
change moves **no pixel and no number** — `M mean`, the validator result and every
dump must be byte-identical to a v190 build. If they differ, something in this
patch is wrong, or `FB.width != 800` at startup on that machine (which would
itself be the finding).
