# Pending Plan Review v223

- plan: docs/PENDING_PLAN_v223.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-now, autonomous invocation #574, this turn)
- timestamp: 2026-08-21

## Design soundness

The plan addresses card S's specific finding (comment wrong in two independent ways) and the design is sound. Card S's premise is correct: (1) `FCommonRenderPasses::SetShaderDataDir()` exists at `FCommonRenderPasses.cpp:290` and has zero callers — re-derived independently this turn via `search_files pattern=SetShaderDataDir path=Engine` returning exactly 1 hit (the definition itself) plus 1 hit in the wrong comment being patched; (2) `g_ShaderDataDirOverride`'s only consumer is `GetShaderDataDir()` whose only caller is `InitBlitResources()` which loads only `BlitVS.sblob`/`BlitPS.sblob` — confirmed by reading `FCommonRenderPasses.cpp:319-322` and `:67-71`. The shared copy of `BilateralDenoise_cs.hlsl` is selected by each consumer's `FBilateralDenoisePass::Initialize(..., InShaderDataDir)` argument, stored at `FBilateralDenoisePass.cpp:44` and used to compose the `.sblob` path at `:48`.

The plan's anti-condition check is also correct: a comment edit is not interactive GPU debug, not a single-line surgical patch (it's a 5-line block), and does not require terminal. The skill IS appropriate here even though the host is single-profile file-only with terminal blocked — those anti-conditions are about the *kind of work*, not the *host capability*; the third anti-condition in the skill reads "single-profile file-only host with terminal blocked AND the work requires real fresh-eyes review," which a comment-only edit does not.

## Plan completeness

Two binding additions before endorsing the patch:

1. **Re-derive the `SetShaderDataDir` zero with the working query mode.** v219/v222 established that `search_files` content-mode returns false zeros on `~/.hermes`; v222's `target=files` is the sound shape. The plan's load-bearing negative ("0 callers") was taken with `pattern=SetShaderDataDir path=Engine` (content mode) — Engine is not `~/.hermes`, but the rule "control every zero" is general. Add a positive control row that proves the query shape works on the same scope: e.g. `pattern="BlitTexture" path=Engine` should return many hits (it does — confirmed this turn: 17 hits across 11 files, all callsites are `FCommonRenderPasses::BlitTexture`).

2. **Anchor the patch on the literal phrase "FCommonRenderPasses uses it unless"** — not on the line preceding the `cbuffer Constants` initialiser. v203's near-miss geometry is real: anchoring on the comment block above a cbuffer can match into the cbuffer braces when the brace counting differs. The plan mentions this risk but the impler's marker must restate it explicitly so it cannot be quietly dropped.

3. **State up front whether the comment edit is itself a build-gated claim.** v211's third-copy finding showed this file compiles; a comment edit cannot perturb the build, so the cycle does NOT require a build. This should be stated explicitly so a future Rule-9 pick does not repeat the v180–v199 "build-gated → deferred" reasoning on this card.

## Feedback for planner (FIX only)

n/a — KEEP with the three additions above binding on the impler's marker (rows 1, 2) and on the cycle summary (row 3).