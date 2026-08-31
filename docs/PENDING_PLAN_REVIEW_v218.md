# Pending Plan Review v218

- plan: docs/PENDING_PLAN_v218.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-566)
- timestamp: 2026-08-21

## Design soundness

The plan solves a real, stated gap rather than hunting an exhausted seam. v200 is on disk and its scope
is `v183-v199`; the delta `v200-v217` is 18 cycles that have never had the same treatment, and the
`- files:` lines of those markers confirm the delta contains **every change shape v200 was built to
catch** — a new cbuffer field (v204/v205), three header edits (v206/v210/v213), a member deletion
(v209), a lifecycle move (v214), and shader edits in a file with three copies (v204/v211). Acceptance
criteria are testable with file tools alone. This is the correct use of a blocked tick: the operator's
one unblocking action is a build, and everything that lowers that build's failure probability has
value that a 566th closure doc does not.

## Plan completeness

Complete on the four checks, with one addition I made at this gate and one correction.

**ADDITION — the plan's check 4 was aimed at the wrong hazard, and testing it produced this cycle's
net-new finding.** The plan framed tri-copy divergence as "an edit landed in one copy and not the one
ShaderMake compiles" (the v182 shape). I tested that framing by asking which copy each consumer
actually loads, and the answer is not the one the source claims:

- `Shader/BilateralDenoise_cs.hlsl:26-32` — the SHARED copy — states in its own header comment that
  `FCommonRenderPasses` uses it "unless a consumer overrides the directory via `SetShaderDataDir()`".
- `SetShaderDataDir` → **4 hits, and none of them is a call**: the definition
  (`FCommonRenderPasses.cpp:290`), the declaration (`FCommonRenderPasses.h:97`), and two comments
  (the shader's own, and `ShaderMakeBuild.py:453`). **Zero call sites tree-wide.**
- The override mechanism the comment describes is not how these consumers select a copy. Both call
  `Initialize(Device, DataDir)` directly (`TestReSTIR_GI_Temporal.cpp:537`), and
  `FBilateralDenoisePass::Initialize` stores that argument at `:44` and combines it into the `.sblob`
  path at `:47-48`. Each test's `*_Data/ShaderMake.cfg` compiles its own local copy
  (`TestReSTIR_GI_Temporal_Data/ShaderMake.cfg:3`), and the shared copy is compiled separately by
  `Shader/ShaderMake.cfg:5`.

So the shared copy's comment describes a dead mechanism. **This is not a compile risk and not a defect
today** — the paths resolve correctly by a different route — but it is a live trap of exactly the kind
v197 and v193 were about: a comment that will be consulted precisely when someone is deciding which
copy to edit, telling them the selection works a way it does not. **Carded as card S, not patched**,
because patching inside an audit cycle destroys the audit's own "zero source modified" row
(v196/v208 precedent).

**CORRECTION — the plan understates check 3's novelty.** `GuideScale` is not merely "the field no
audit has seen": it is the only cbuffer field in the codebase that must agree across **three** shader
copies rather than two, and the third copy deliberately **disagrees in name** (`GuideScale_Unused`,
`TestCornellBoxGI_Data/BilateralDenoise_cs.hlsl:26`). A name-equality sweep across copies would
report that as a divergence and be wrong. The check must be on **slot position and size**, not name.

## Feedback for planner

None blocking. Proceed with the addition and correction folded in.
