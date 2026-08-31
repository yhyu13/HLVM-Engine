# Pending Plan Review v206

- plan: docs/PENDING_PLAN_v206.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-552)
- timestamp: 2026-08-30

## Design soundness

The plan is sound and its conditional structure is right: Q1 gates Q2, and Q1 is
already settled with a controlled zero rather than a bare one (`GB(` → 0 in
`TestReSTIR_GI_Temporal_Data/ReBLUR_cs.hlsl`, against 22 in the same directory
from the same query). I re-ran that query myself and confirm the partition: the
22 hits fall in `BilateralDenoise_cs.hlsl`, `ReSTIR_Spatial_cs.hlsl` and
`ReSTIR_Temporal_cs.hlsl` — three of the four Phase-D-aware kernels — and ReBLUR
is the one that has none. That is the correct shape of evidence for this claim.

I also independently confirm the absence of a scale *field*, which is the
stronger form of Q1: `FReBLURConstants` (`FReBLURPass.h:31-52`) is a 340-byte
struct with a `static_assert` pinning its size, and there is no `GuideScale`,
`GBufferScale` or equivalent anywhere in it. So ReBLUR cannot be applying a
scale silently through a differently-named field — a possibility the plan did
not name and which would have made a `GB(` sweep insufficient on its own.

## Plan completeness

Two corrections, one of which changes what the impler should do.

**(1) Risk 2 is already resolved, and the plan should not spend a cycle
re-determining it.** The plan asks the impler to determine whether the control's
`ReBLURDesc.OutputWidth = CurrentFBInfo.width` (`TestCornellBoxGI.cpp:1464`) is
correct-because-recreated or defective. It is **defective, it is already carded,
and it is build-gated** — this is a member of card L's set, not a new finding.
`ReBLURHistoryTexture` → 13 hits across that file, and the partition is decisive:
creations at `:932`/`:934` inside the init-time `if (CVar_r_ReBLUR_Enable)` block,
teardown nulls at `:1085-1086` and `:1777-1778`, uses at `:1428`/`:1460`/`:1470`/
`:1471`, declaration at `:1869`. **Nothing in the resize block.** Card L named
"ReBLUR history ×2" among its 14 uncontained textures, so this is that instance
seen from the other end — the extent source is swapchain-derived *and* the
resource it sizes is never recreated, which is the same defect described twice.
The impler must therefore treat the control as **out of scope by precondition**,
not as an open question. This preserves both the v196 no-touch rule and card L's
correct deferral, and it removes the sole path by which this cycle could have
edited the known-good control.

**(2) The plan's expectation for Q2 is under-evidenced as written, and I am
upgrading the required standard rather than the conclusion.** The plan reasons
that ReBLUR runs after the Phase-D resolve so its input is full-res. That is
true but it is an argument from *ordering*, and ordering is exactly what v183
showed can silently change (Phase D reordered the pipeline and left four passes
indexing the old way). The impler must establish the extent of all five operands
**from the creation site that sizes each**, not from pipeline position:
`CurrentRadianceTexture`, `HistoryTexture`, `DepthTexture`,
`NormalRoughnessTexture`, `OutputTexture`, plus the `OutputWidth/Height` pair.
Six quantities, six creation sites, one row each. If all six are `W`/`H`, ReBLUR
is clean **by enumeration**, which is a materially stronger claim than clean
by ordering and is the only form that survives the next reordering.

## What the plan gets right and should not lose

The plan's framing of the likely output — a **contract divergence**, not a
defect — is the most valuable thing in it and I want it preserved verbatim
through implementation. v205 wrote into `FBilateralDenoisePass.h` that guides
need *not* match `OutputWidth`. If ReBLUR indexes raw, its guides *must*. Two
sibling classes in one directory with opposite guide contracts, one documented
and one not, is a trap of the same family as the three camouflage instances
already on record — and the documented one actively misleads about the
undocumented one. A reader who has just been taught v205's invariant carries
precisely the wrong rule into ReBLUR.

## Feedback for planner

None blocking. Corrections (1) and (2) are directives to the impler, recorded
here rather than requiring a plan revision, since neither changes the approach —
(1) removes work, (2) raises the evidentiary bar on work already planned.
