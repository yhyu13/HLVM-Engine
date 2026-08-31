# Pending Plan Review v184

- plan: docs/PENDING_PLAN_v184.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-531)
- timestamp: 2026-08-30

## Design soundness

The plan identifies a real, mechanically-checkable defect and the fix is the
minimal one that removes the failure mode rather than papering over it.

I re-derived the packing arithmetic independently rather than accepting the
planner's table, because it is the load-bearing claim and an off-by-one in it
would invalidate the whole cycle.

**Independent re-derivation.** `ReSTIR_Temporal_cs.hlsl:24-25` are two
`float4x4` = 32 floats, register-aligned, so scalar packing starts at float
32 with no ambiguity. Counting forward from `:26`: `OutputSize`(32,33),
`RcpOutputSize`(34,35), `FrameIndex`(36), `MaxM`(37), `DepthThreshold`(38),
`NormalThreshold`(39), `DebugVis`(40), `SceneYaw`(41), `PrevSceneYaw`(42).
Float 43 is register 10 slot .w. `Pad[]` is an array, and the HLSL rule is
that each array element begins a new 16-byte register — so it cannot occupy
43 and is pushed to 44. `Pad[1]` takes 48. `GBufferScale` follows at 52.

The C++ side writes 43/44/45 (`FReSTIRPass.cpp:449-451`, offset having been
incremented 11 times from 32). So the three fields land at 43,44,45 while the
shader reads 44,48,52. Confirmed: `nearP` reads what C++ wrote as far (50.0),
`farP` reads memset zero, `GBufferScale` reads memset zero.

**The most important consequence is correctly identified and I verified it
directly.** `ReSTIR_Temporal_cs.hlsl:74` and `ReSTIR_Spatial_cs.hlsl:54` both
compute `int s = max(int(gConstants.GBufferScale), 1)`. With the temporal
constant arriving as 0, `s` clamps to 1 and `GB(p)` returns `p` — the v183
temporal fix is **inert on the temporal pass**. v183's audit staked its
hypothesis on `M mean` rising; had this shipped, a null result would have
been recorded as a refutation of a hypothesis that was never actually tested.
That is the strongest argument for this cycle and it is stated in the plan.

**Spatial exemption verified, not assumed.** `ReSTIR_Spatial_cs.hlsl:16-27`
contains no array. `GBufferScale` is float 9 on both sides. So the spatial
half of v183 is live and correct today, and this plan correctly declines to
touch it beyond leaving it alone. Good scoping — a less careful plan would
have "fixed" both symmetrically and introduced a regression.

## Plan completeness

All five edit sites are named with file and line, and they are exactly the
sites the tree contains: `search_files pattern="\.Pad\["` over
`Engine/Source/Runtime` returns 6 matches in 3 files — `TestReSTIR_GI_Temporal.cpp:976-977`,
`ReSTIR_Temporal_cs.hlsl:140-141`, `FReSTIRPass.cpp:449-450`. The plan
updates all six plus the two declarations. Nothing is missed and nothing
invented.

Cornell sibling handling is correct and non-obvious: `TestCornellBoxGI_Data/
ReSTIR_Temporal_cs.hlsl:15` declares `float Pad[3]` and never reads it, so
the three extra floats now written by the shared `FReSTIRPass.cpp` land in
declared, unread padding. The plan explicitly says do not edit it. Right call
— editing it would be scope creep with no benefit.

Buffer capacity re-checked: high-water 46 floats against `ConstantsData[64]`
and a 256-byte cbuffer. Fine.

## One point I pushed back on and then accepted

I considered whether `packoffset` would be the better fix, since it states
intent explicitly. I now agree with the plan's rejection of it: `packoffset`
creates a second source of truth that must be maintained in lockstep with a
field-by-field C++ marshaller, which is precisely the class of desync that
produced this bug and the v183 one before it. Plain scalars make
"C++ order == HLSL order" checkable by inspection, which is the invariant the
marshaller actually depends on. Keep as planned.

## Residual risk (acknowledged, not blocking)

This is a **production-path** change and can move the display image and the
validator in either direction. It is not `#ifdef`-confined. Combined with
v183 (also production-path, and until now partly inert), the working tree now
carries two unverified production changes. The impler must not report any
acceptance gate as PASS, and the falsifiable `M mean` prediction must be
reported honestly whichever way it goes.

## Feedback for planner

None. Proceed to impler.
