# Pending Plan Review v188

- plan: docs/PENDING_PLAN_v188.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-535)
- timestamp: 2026-08-30

## Design soundness

The plan's central move — **rejecting the card's own prescribed fix** — is the
part I checked hardest, because a planner that overrules its input card is
either doing the most valuable thing available or rationalising scope creep.
Re-derived from source independently, not from the plan's quotation:

- `search_files pattern="SceneYaw"` over `Engine/Source/Runtime/Test` → 6 hits,
  in `TestReSTIR_GI_Temporal.cpp:993-994` and
  `TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl:33,34,157,158`. **Zero in
  any `TestCornellBoxGI` file.** Confirmed: the Cornell copy is missing the two
  Phase-C fields, and card C does not mention them.
- `FReSTIRPass.cpp:441-442` writes `SceneYaw`/`PrevSceneYaw` unconditionally,
  between `DebugVis` (`:440`) and `NearPlane` (`:454`). No `if`, no caller
  discrimination — same unconditional-marshaller shape v187 turned on.

So the card's three-scalar replacement really would have named floats 41/42/43
`NearPlane`/`FarPlane`/`GBufferScale` while C++ writes `SceneYaw`/`PrevSceneYaw`/
`NearPlane` there. The plan is right, the card is wrong, and the correction is
the cycle's value. **This is the second consecutive cycle in which the card was
accurate about the symptom and wrong about the remedy** (v187: right mismatch,
missed the coupled lifetime defect). Worth recording as a pattern: cards opened
at a *review* gate describe what was seen from that gate's vantage, and the
subsequent planner must re-derive the fix from source rather than execute the
card's prescription.

## Plan completeness

Checked the three things most likely to be missing.

**1. Does the enlarged struct still fit the buffer?** The plan flags this as a
risk but does not resolve it; I did. `FReSTIRPass.cpp:317` `BufferDesc.byteSize
= 256`, and all three marshallers use `float ConstantsData[64]` (`:351`, `:424`,
`:532`) with `writeBuffer(..., sizeof(ConstantsData))` = 256 bytes exactly. The
temporal struct goes 41 → 45 declared floats = 180 bytes. **Headroom is 64
bytes; no overflow.** The risk is real but discharged — downgrade it in the
impl notes rather than carrying it as an open unknown.

**2. Is the read-inertness claim actually exhaustive?** The plan lists 7
`gConstants` reads in the Cornell temporal shader. I re-ran the query myself:
`:38`, `:64`, `:69`, `:73`, `:101` (two on one line), `:131`, `:142` — 7 hits,
none touching any of the five fields. The claim is sound and, importantly, the
plan states the *weaker* correct claim (read-inert, not byte-inert) and
explicitly declines the stronger one. That is the right instinct; v187 could
legitimately claim byte-inertness and this one cannot.

**3. Are the constants derived or guessed?** `TestCornellBoxGI.cpp:1276`
`glm::perspectiveLH_ZO(glm::radians(90.0f), aspectRatio, 0.01f, 10.0f)` — the
0.01/10.0 are read off the call site, and the plan explicitly refuses to copy
the sibling's 0.001/50.0. `GBufferScale = 1.0f` follows from `:1585-1586`
dispatching at `CurrentFBInfo` while `:1578-1579` bind full-res MRTs. Derived,
not guessed. Good.

## One thing the plan should have said and did not

The plan asserts scalars "pack tightly, so no register displacement occurs."
True here, but the reason is worth stating precisely, because this is the third
cycle to lean on it: floats 41,42,43 occupy register 10 slots `.y/.z/.w`, and
44,45 begin register 11 at `.x/.y`. **No member straddles a 16-byte boundary**,
which is the actual precondition — HLSL will not split a scalar, and would have
bumped a `float2` or `float4` starting at 43. Since all five are scalars the
question does not arise, but if anyone later appends a vector to this tail, the
straddle rule bites before the array rule does. Not a FIX; a note the impler
should carry into the comment.

## Feedback for planner (FIX only)

None. KEEP.

## Caveat

Single-profile host: planner and critic are the same model
(`six-role-pipeline §Anti-patterns §7`). I re-derived every load-bearing claim
from source with my own queries rather than reading the plan's citations, which
is what makes this more than a restatement — but it is a self-check, not fresh
eyes. Nothing here was built, compiled, or run.
