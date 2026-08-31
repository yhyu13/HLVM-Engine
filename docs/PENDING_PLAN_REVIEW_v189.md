# Pending Plan Review v189

- plan: docs/PENDING_PLAN_v189.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-536)
- timestamp: 2026-08-30

## Design soundness

The plan solves the stated problem and — more importantly — it is the first
cycle in four to actually *test* card B's deferral reasons instead of repeating
them. I re-derived both refutations independently rather than accepting them.

**Refutation 1 (dump deadness) — CONFIRMED, and I re-ran the queries.**
`AccumInput = DenoisedTexture` (`:1168`) is inside `if (bReBLURInitialized &&
!bBypass)` (`:1112`), *after* ReBLUR has overwritten the texture at `:1148`. I
checked the else-path the plan asserts: `:1111` initialises `AccumInput` to
`bBypass ? FullResGIRaw : FullResSpatial`, and `search_files` for `AccumInput`
returns 8 hits — the only assignment other than `:1111` is `:1168`. So when
ReBLUR is disabled there is **no** path by which bilateral output reaches
`:1191 .SetTextureSRV(0, AccumInput)`. Confirmed by query, not by reading the
plan: `denoised` → 0 hits in `validate_restir_gi.py`, 0 hits in
`v176-recipe.sh`. Per the tick-526 alternation rule I ran these as separate
single-term queries.

**Refutation 2 (barrier flush is grid-independent) — CONFIRMED.**
`FBilateralDenoisePass.cpp`: binding set `:167-176`, `setComputeState` `:185`,
`dispatch` `:186`. `dispatchX/Y` are computed at `:179-180` and consumed only at
`:186`. Nothing in the barrier path reads them. The flush survives verbatim.

**The defect itself — CONFIRMED.** Input `OutputTexture` is 400x300
(`:1564-1566` from `:1560-1561`, `W=800` at `:106`); `Bd.OutputWidth = FB.width`
= 800 at `:852`. Both grid and `TexelSize` derive from it
(`FBilateralDenoisePass.cpp:179-180`, `:158-159`); the shader inverts `TexelSize`
to recover `outputSize` (`BilateralDenoise_cs.hlsl:60`) and uses it for the
early-out `:62` and the neighbour bounds test `:87`.

## Plan completeness

Checked for the failure mode that bit v187 and v188 — card right about the
symptom, wrong about the remedy. **This plan does not inherit its fix from the
card.** Card B proposed no remedy at all; the plan derives one from the input
texture's own creation extent. Good.

Two completeness checks I ran that the plan did not state:

1. **Initialization order.** The plan claims `HalfResWidth` is assigned before
   `:852`. `:1562` is in resource setup; `:852` is in `Render`. I confirmed the
   stronger in-function fact: **`:793` already uses `HalfResWidth`, 59 lines
   before `:852`, in the same function.** If `HalfResWidth` were zero at `:852`
   it would already be zero at `:793` and the GI trace would have a zero grid.
   It does not. Order is sound.
2. **The other caller is genuinely unaffected.** `TestCornellBoxGI.cpp:1478-1488`
   passes `CurrentFBInfo.width/height` with `HDRTexture` and
   `DenoisedHDRTexture` both at `GBufferWidth x GBufferHeight` (`:867-868`,
   `:885-886`, and the resize path `:1233/:1237` recreates both from one `Desc`).
   Cornell is already self-consistent and this cycle must not touch it —
   correctly scoped out.

## What I checked and found NOT to be a problem

- **Shader copies.** Three `BilateralDenoise_cs.hlsl` exist (Runtime/Shader,
  TestReSTIR_GI_Temporal_Data, TestCornellBoxGI_Data). This is the v182
  dead-copy trap's natural habitat. **No shader is edited**, so the trap cannot
  fire. Noted so a later reader does not re-raise it.
- **`FDesc` defaults.** `OutputWidth = 0` (`FBilateralDenoisePass.h:29`) with a
  fallback to the *output texture's* extent (`:142-147`) — full-res, i.e. the
  current wrong value. Leaving the field unset would have been the wrong fix.
  The plan sets it explicitly. Correct.

## The plan's best property

It states plainly that the fix does **not** make the pass coherent — guides stay
full-res, output stays full-res, so post-fix the pass filters a quadrant. It
justifies accepting that only by the output's proven deadness, and it routes the
honest full fix (replace the dispatch with `commitBarriers()`, the idiom already
used at `:1157`) to its own cycle rather than smuggling it in. Given that this
lineage's recurring failure is patches later cited as evidence of correctness
they never established, that disclosure is the difference between KEEP and FIX.

## Residual risk accepted

The `denoised` PNG will change content. No gate reads it. The plan says so and
says it must not be reported as regression or improvement. Accepted.

## Caveat

Single-profile host: planner and criticer are the same model. This review
re-executed the plan's load-bearing queries against source rather than reasoning
about its prose, which is the only available substitute for fresh eyes
(`six-role-pipeline §Anti-patterns §7`).
