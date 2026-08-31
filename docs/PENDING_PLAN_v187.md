# Pending Plan v187

- task: NEW card A (opened by tick-533) — LIVE C++/HLSL desync in the known-good control test `TestCornellBoxGI`
- source: no bundle — direct edit
- approach: Align the Cornell copy of `FReSTIRSpatialConstants` with the shared
  C++ header (`FReSTIRPass.h:62-73`) by adding the missing `GBufferScale` field
  and demoting `float2 Pad` to `float Pad`, matching the Temporal-test copy
  verbatim. **Plus a defect the card did not identify** (see below): value-initialize
  the three Cornell ReSTIR constant structs so the field-by-field marshaller stops
  reading indeterminate memory. Touches
  `Test/TestCornellBoxGI_Data/ReSTIR_Spatial_cs.hlsl` and `Test/TestCornellBoxGI.cpp`.
- diff_estimate: +6 / -4 lines
- skip_plan_review: no
- test_strategy: file-only static verifier (role #5) enumerating the three-way
  declaration agreement (C++ header ↔ both HLSL copies), the marshaller offset
  walk, the value-init sites, and compiled-path confirmation via `ShaderMake.cfg`.
- risks: see §Risks

## The card's claim, re-derived from source (CONFIRMED)

`FReSTIRPass::DispatchSpatial` (`FReSTIRPass.cpp:535-547`) marshals field-by-field
into a flat `float ConstantsData[64]`, writing `DebugVis` at float 8 and
`GBufferScale` at float **9**, unconditionally, for every caller.

- Temporal test shader (`TestReSTIR_GI_Temporal_Data/ReSTIR_Spatial_cs.hlsl:16-27`):
  `... DebugVis; GBufferScale; float Pad;` — agrees with the header.
- Cornell shader (`TestCornellBoxGI_Data/ReSTIR_Spatial_cs.hlsl:10-20`):
  `... DebugVis; float2 Pad;` — **no `GBufferScale` field at all.**
  `search_files pattern="GBufferScale"` over the whole Runtime tree returns 14
  hits, none in any `TestCornellBoxGI_Data` file.

So float 9 lands in Cornell's `Pad.x`. `search_files pattern="gConstants.Pad"`
over Runtime → **0 hits**, so it is read nowhere: swallowed, not corrupting.
The file is live, not dead — `TestCornellBoxGI_Data/ShaderMake.cfg:7` compiles it.

Card A is accurate on every point I could check.

## What the card missed — the reason this cycle is worth more than 2 lines

The card frames this as a declaration mismatch. Underneath it is a **read of
indeterminate memory**, and it is not confined to the spatial struct.

All three Cornell ReSTIR constant structs are declared with **no initializer**:

- `TestCornellBoxGI.cpp:1513` `ReSTIR::FReSTIRConstants GenConstants;`
- `TestCornellBoxGI.cpp:1551` `ReSTIR::FReSTIRTemporalConstants TempConstants;`
- `TestCornellBoxGI.cpp:1602` `ReSTIR::FReSTIRSpatialConstants SpatConstants;`

These are aggregates with no default member initializers, so default-initialization
leaves every member **indeterminate**. Cornell then assigns a prefix of each struct
and stops at `DebugVis` (`:1522`, `:1564`, `:1611`). It never assigns
`NearPlane`, `FarPlane` or `GBufferScale` on the temporal struct, nor
`GBufferScale`/`Pad` on the spatial struct.

The marshallers read those unassigned fields anyway:

- `FReSTIRPass.cpp:454-456` writes `NearPlane`/`FarPlane`/`GBufferScale` at
  floats 43/44/45 (added by v183/v184 for the Temporal test).
- `FReSTIRPass.cpp:547` writes `GBufferScale` at float 9.

So **four indeterminate floats per frame are copied into a GPU-visible buffer**
in the control test. Contrast the Temporal test, which value-initializes
(`TestReSTIR_GI_Temporal.cpp:1042` `FReSTIRSpatialConstants SC{};`) *and* assigns
every field (`:1005`, `:998-999`, `:1051`).

Whether those floats are currently read by the Cornell shaders — they are not,
per the offset walk below — is a property of today's shader source, not of the
C++. The C++ read itself is already undefined behaviour, and the value is
already leaving the process. This is the same latent-trap class as v184/v186 but
one level down: v186 fixed a *kind* mismatch that would bite on the next appended
field; this is a *lifetime* defect that is live now and merely unobserved.

## Why the naive fix would make the control WORSE

The obvious patch is "add `float GBufferScale;` to Cornell's spatial shader so
all three sides agree." Applied alone, that is a **regression**:

Today float 9 lands in Cornell's unread `Pad.x` and is discarded. Add the field
and Cornell's shader now has a `GBufferScale` slot fed by
`SpatConstants.GBufferScale`, which Cornell never assigns — so the shader would
be declaring a *readable* field backed by indeterminate memory. If anyone later
adds a `GB()` helper to the Cornell shader (exactly the direction v183 took the
Temporal copy), it reads garbage, and `max(int(scale),1)` would mask it into a
plausible-looking wrong answer rather than an obvious one.

**The two edits are therefore not independent and must land together**, with the
value-init first in the reasoning order. That coupling is the substance of this
plan; either half alone is worse than doing nothing.

## Offset walk (why nothing moves today)

HLSL puts each constant-buffer **array** element on its own 16-byte register;
vectors pack normally (the v184 rule, corrected by v186).

Cornell spatial, current: `OutputSize` 0-1, `RcpOutputSize` 2-3, `NormalThreshold` 4,
`DepthThreshold` 5, `MaxM` 6, `SpatialRadius` 7, `DebugVis` 8, `float2 Pad` 9-10.
C++ writes 0..9. Float 9 → `Pad.x`, unread.

Cornell spatial, after: `... DebugVis` 8, `GBufferScale` 9, `float Pad` 10.
C++ writes 0..9 — identical bytes at identical offsets. **Byte-for-byte
unchanged on the wire**; only the name the shader gives float 9 changes, and it
still reads it nowhere. Combined with value-init, float 9 goes from indeterminate
to a defined `1.0f`.

Cornell temporal is left alone deliberately: its shader declares `float Pad[3]`
(`:15`) which HLSL places at 44/48/52, while C++ writes 43/44/45. Only float 44
overlaps, at `Pad[0]`, unread. Value-init makes those three writes defined; the
declaration mismatch there is a **separate card**, not this one — different
struct, and bundling it would break one-variable-per-experiment.

## Files

- Modify: `Engine/Source/Runtime/Test/TestCornellBoxGI_Data/ReSTIR_Spatial_cs.hlsl:18-19`
- Modify: `Engine/Source/Runtime/Test/TestCornellBoxGI.cpp:1513,1551,1602,1611`

## Acceptance criteria (testable file-only)

1. `TestCornellBoxGI_Data/ReSTIR_Spatial_cs.hlsl` declares `GBufferScale` then a
   scalar `Pad`, in that order, matching `FReSTIRPass.h:70-72` and the Temporal copy.
2. `search_files pattern="float2 Pad;"` no longer matches any `ReSTIR_*` shader.
3. All three Cornell ReSTIR struct declarations use `{}`.
4. `SpatConstants.GBufferScale` is explicitly assigned `1.0f` (Cornell dispatches
   the spatial pass at full res — `:1620-1621` pass `CurrentFBInfo.width/height`,
   the same value its GBuffer MRTs use — so the correct ratio is exactly 1).
5. The Cornell shader still contains no `GB(` helper and no read of
   `gConstants.GBufferScale`: this patch aligns declarations, it does not change
   sampling.
6. `TestReSTIR_GI_Temporal_Data/` is untouched, so the v183+v184+v185 chain
   awaiting its single operator run is not perturbed.

## Risks

- **Cannot be compiled or run here.** `terminal` is denied categorically by tirith
  (probes this tick below). slangc acceptance of the edited struct is UNVERIFIED.
- Requires a `TestCornellBoxGI` rebuild to confirm the control still passes. That
  is operator-side.
- The `1.0f` in criterion 4 is a semantic choice, not a wire change (see offset
  walk); if Cornell were ever moved to a half-res spatial dispatch this line
  becomes load-bearing and must be recomputed like `TestReSTIR_GI_Temporal.cpp:1051`.
- Cornell's **temporal** struct retains a real declaration mismatch. Left open as
  a follow-up card rather than fixed blind.
