# Pending Plan Review v211

- plan: docs/PENDING_PLAN_v211.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-557)
- timestamp: 2026-08-30

## Design soundness

The plan solves a real, source-decidable defect and its acceptance criteria are
falsifiable from files alone. I re-derived every load-bearing claim rather than
accepting it, and **all held**. Two are now stronger than the plan stated, and
one framing needs a correction that does not change the remedy.

**Re-derived independently:**

1. **Three copies, and the plan under-states its own corroboration.**
   `search_files target=files pattern="BilateralDenoise_cs.hlsl"` → 3, as the
   plan says. But I also checked the sibling entries of the same cfg:
   `JointBilateralUpsample_cs.hlsl` → **1**, `HBAO_cs.hlsl` → **1**. So
   `BilateralDenoise_cs.hlsl` is **the only duplicated file in that
   directory** — which makes the three-copy situation an *accident of history*
   rather than a project convention, and defeats the most likely objection
   ("shared shaders are always mirrored, this is normal"). It is not normal;
   it is unique in its own directory.

2. **It is not merely configured to compile — it HAS compiled.**
   `Engine/Source/Runtime/Shader/BilateralDenoise_cs.sblob` is **on disk**,
   alongside `HBAO_cs.sblob`, `JointBilateralUpsample_cs.sblob`,
   `BlitVS.sblob`, `BlitPS.sblob`. The plan argued from cfg + `build.ninja`,
   i.e. from *intent to build*. The blob is the artifact, i.e. *evidence of a
   completed build*. **A stale-shape shader has been compiled to SPIR-V and is
   sitting in the tree as a loadable default.** Strictly stronger than the
   plan's own claim.

3. **`GuideScale` → 0 in the third copy, correctly controlled.** The plan's
   control (`cbuffer` → 1 same file) is adequate. I add a second of a
   different shape: the directory-scoped query enumerated **all 11 files** in
   `Engine/Source/Runtime/Shader` including the target, so this is not a
   v210 truncated-enumeration false zero, and `limit_reason` is absent, so it
   is not a v209 timeout false zero. Both mechanisms ruled out explicitly.

4. **The consumer claim holds.** `FBilateralDenoisePass::Initialize` takes
   `InShaderDataDir` and loads the blob from it (`:44`, `:48`); both consumers
   pass their own data dir (`TestReSTIR_GI_Temporal.cpp:537`,
   `TestCornellBoxGI.cpp:901`). **Latent, not live** is the honest severity and
   the plan states it without inflation. Good.

5. **Layout-neutrality holds.** Both shapes are 8 floats: `float2` + 3 named
   + 3 tail. The edit renames a tail slot in place. The v184/v200 rule (never
   an array in the tail; never displace the tail) is not engaged.

## Correction — one framing claim is over-stated

The plan calls this the **"seventh false-instrument mechanism"** and says it is
"defeated only by not assuming you know how many copies a file has."

That is half right. A `search_files target=files` on the filename is exactly
the query that finds it, and that query is neither exotic nor new — v199 and
v206 both used it. So the mechanism is **not** a limitation of the
instrument; it is a **failure to run any query at all**, because eleven cycles
inherited "there are two copies" as a fact from v182 and never re-derived it.

That is a **different and more embarrassing** failure than the six catalogued
mechanisms, and it should be recorded as what it is: not *"the instrument lied"*
but ***"nobody asked."*** The standing rule that follows is v195's, already on
the books and simply not applied here — *a marker's description of code is
evidence about its author, not about the code* — extended one step: **that
applies to cardinality claims inherited across cycles, including ones every
subsequent cycle repeated.** Eleven repetitions of "both copies" did not make
it two copies.

**This is a correction to the audit framing, not to the remedy.** The plan's
patch is unaffected, so it does not warrant FIX.

## Plan completeness

Complete for its stated scope. Three checks I verified the plan had already
closed rather than assumed:

- **`GB()` absent from the whole directory** — `GB(` → 0 across all 11 files
  in `Engine/Source/Runtime/Shader`, so the helper cannot collide with an
  existing symbol in any file the `Common_ShaderMake` target compiles.
- **The over-reach risk is correctly identified and is the right one.**
  Routing `t_Input`/`u_Output` through `GB()` would be wrong at *every* scale
  including 1 — it is the one edit in scope that could convert a latent defect
  into a live one. The plan flags it explicitly. Verified the primary keeps
  both on the raw coord (`:84`, `:125`, `:131`).
- **Deferring `GIAccumulate_cs.hlsl` is correct**, and for the reason the plan
  gives: bundling would make this cycle's own three-copy enumeration
  unverifiable. Same discipline as v191-v195.

## Feedback for planner

None blocking. Carry the framing correction above into the commit marker so
the audit lineage records the mechanism accurately, and carry the `.sblob`
evidence, which is stronger than what the plan argued from.
