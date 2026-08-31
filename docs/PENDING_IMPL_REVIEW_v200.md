# Pending Impl Review v200

- plan: docs/PENDING_PLAN_v200.md
- commit: docs/PENDING_COMMIT_v200.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-546)
- timestamp: 2026-08-30

## plan_fidelity_check

The impl matches the plan and declares one deviation: it added risk classes (c)
and (d) beyond the plan's (a) and (b). Both are justified. (c) was *required* by
the plan review, so adding it is compliance rather than deviation. (d) is the
impler's own addition and is the better of the two — the v182 dual-copy hazard
is the one failure mode in this lineage that a layout check structurally cannot
see, because both copies can be internally coherent and still disagree with each
other. Accepted.

Zero source files modified, as planned.

## Defect found in the impl marker: an off-by-one, corrected here

The impler's overflow check claimed the temporal marshaller writes "13 scalars =
**45 of 64**, 19 floats of headroom." I re-counted from the source rather than
reading the sentence, and it is **14 scalars = 46 of 64, 18 floats of headroom**:

`:432-442` is eleven scalars (OutputSize ×2, RcpOutputSize ×2, FrameIndex, MaxM,
DepthThreshold, NormalThreshold, DebugVis, SceneYaw, PrevSceneYaw), then
`:454-456` is three more (NearPlane, FarPlane, GBufferScale). 11 + 3 = 14, on top
of 32 floats of matrices.

**The conclusion survives — 46 < 64, no overflow — so this is not a FIX.** But
the lineage's own standard (v191: "counts are not invariants, sets are") says a
miscounted count gets recorded even when it is harmless, because the next cycle
may reuse the number rather than the reasoning.

## The correction produces a stronger result than the impler claimed

Re-deriving the offsets exactly, `NearPlane` lands at float **43**, `FarPlane`
at **44**, `GBufferScale` at **45** (32 matrix floats + 11 scalars = 43).

That is independently checkable against a number written in the source by a
*different* cycle: `FReSTIRPass.cpp:452` states "C++ writes 43/44/45," and
`TestCornellBoxGI_Data/ReSTIR_Spatial_cs.hlsl:24-27` independently derives
"DebugVis at float 40 … against C++ writes at 41..45."

I also walked the HLSL side under the std140-style packing rules rather than
assuming symmetry: matrices occupy 0-31; `float2 OutputSize` starts at 32 (a
register boundary, so no straddle bump) taking 32-33; RcpOutputSize 34-35;
then seven scalars 36-42; NearPlane **43**, FarPlane **44**, GBufferScale **45**.

**Five independent derivations agree on 43/44/45** — the C++ struct, the
marshaller's own comment, the primary HLSL copy, the control HLSL copy, and my
own from-scratch offset walk. That is a materially stronger claim than the
impler's four-way *ordering* check, because ordering agreement would still permit
a straddle bump to displace the tail; this pins the absolute float indices.

## Security scan

- [x] No hardcoded secrets — no source modified
- [x] No shell injection — no code
- [x] No eval/exec — n/a
- [x] No SQL injection — n/a

## Self-review checklist

- [x] Validation: every negative in the impl is paired with a positive control
      of the same query shape; I spot-checked the `RenderGBuffer(` = 2 zero
      against the unparenthesised 13 and it holds
- [x] Error handling: n/a (audit)
- [x] Tests: no test files produced; `produces_test_files: no` is accurate, so
      `skip_impl_review` was still correctly set to `no` (HARD INVARIANT #2 not
      engaged, but the gate ran anyway — correct)

## What this review did NOT do

Did not build, run, compile, or view any image. The audit's subject is whether
the chain *will* build; this review confirms the audit's method and corrects its
arithmetic. **It remains a static analysis and is not a substitute for the build.**

## Feedback for impler (FIX only)

n/a — KEEP. The off-by-one is corrected in this marker; no re-work needed.
