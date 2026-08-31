# Pending Plan Review v187

- plan: docs/PENDING_PLAN_v187.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-534)
- timestamp: 2026-08-30

## Design soundness

The plan solves the card, and it corrects the card's framing in a way that
matters. I re-derived both load-bearing claims from source rather than accepting
the plan's word.

**Claim 1 — Cornell spatial is missing `GBufferScale`.** Verified directly:
`TestCornellBoxGI_Data/ReSTIR_Spatial_cs.hlsl:18-19` is `DebugVis; float2 Pad;`
against `FReSTIRPass.h:70-72` `DebugVis; GBufferScale; Pad;`. The unconditional
write is at `FReSTIRPass.cpp:547`. Live path confirmed at that test's
`ShaderMake.cfg:7`. CONFIRMED.

**Claim 2 — the indeterminate-read defect.** This is the plan's addition and it
holds. `TestCornellBoxGI.cpp:1513/1551/1602` declare the three structs with no
initializer; the structs (`FReSTIRPass.h:19-73`) have no default member
initializers, so members are indeterminate, and Cornell assigns only a prefix of
each. CONFIRMED.

**I re-derived the plan's count of four, because an inflated number here would
be the kind of thing that gets quoted for 500 ticks.** It is exactly four, and
the plan's arithmetic is right for the right reason:

- Generation marshaller (`FReSTIRPass.cpp:354-363`) stops at `DebugVis`, offset 8.
  `Pad0`/`Pad1` are never read → contributes **0**, despite being unassigned.
- Temporal marshaller (`:454-456`) reads `NearPlane`, `FarPlane`, `GBufferScale`,
  none of which Cornell assigns (`:1554-1564` stops at `DebugVis`) → **3**.
- Spatial marshaller (`:547`) reads `GBufferScale`, unassigned → **1**.

Four. The plan did not overstate by lumping in the generation pad.

**Claim 3 — the naive fix is a regression.** This is the strongest part of the
plan and I agree with the reasoning. Adding the field alone converts a
discarded write into a *named, readable* field backed by indeterminate memory,
in the exact file v183 would next want a `GB()` helper in, where
`max(int(scale),1)` would launder garbage into a plausible wrong answer. The
coupling argument is correct and the ordering (value-init first) is correct.

## Plan completeness

- Files, offsets and acceptance criteria are exact and independently checkable.
- The offset walk is right, including the corrected v186 rule that `float2` is a
  vector and packs at 9/10 (not an array). Post-patch the wire bytes are
  identical; only float 9's name and definedness change. Good — this is the
  property that makes the patch safe to land un-run.
- `GBufferScale = 1.0f` is justified from the call site: `:1620-1621` dispatch
  spatial at `CurrentFBInfo.width/height`, the same resolution as the GBuffer
  MRTs it samples, so the ratio is exactly 1. Not a guess.
- Scope discipline is right. Cornell's **temporal** struct has a genuine
  `float Pad[3]` (`:15`) vs C++ `NearPlane/FarPlane/GBufferScale` mismatch;
  deferring it to its own card is correct, and value-init makes those three
  writes defined in the meantime, which is the safe half of that fix.
- `TestReSTIR_GI_Temporal_Data/` untouched → the v183+v184+v185 chain awaiting a
  single operator run cannot be perturbed. Load-bearing and explicitly stated.

## Reservations (not blocking)

1. Criterion 5 ("no `GB(` helper added") is stated as an acceptance check but is
   really a scope fence. Keep it — a future tick reading only this marker could
   otherwise conclude Cornell was made half-res-aware.
2. The patch cannot be compiled here. Landing an un-compiled shader edit is a
   real risk, mitigated only by the edit being a two-token change to a struct
   declaration of a form already compiling in the sibling file. Must be recorded
   as UNVERIFIED, not glossed.
3. `+6/-4` is plausible: 2 lines in the shader, 3 `{}` insertions, 1 assignment.

## Feedback for planner (FIX only)

None. Proceeding to impler.
