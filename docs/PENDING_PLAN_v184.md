# Pending Plan v184

- task: fix HLSL cbuffer array-packing mismatch introduced/exposed by v183 —
  `float Pad[2]` in the temporal cbuffer does NOT pack as two consecutive
  floats, so `GBufferScale`, `near` and `far` all land at wrong offsets
- source: no bundle — direct edit
- planner: agent_1_planner (tick-531)
- skip_plan_review: no
- diff_estimate: +6 / -6 lines (3 files, no size change intended)

## Why this cycle exists (Rule 9: v183 audit closed ALL_KEEP)

v183 landed a real fix (half-res dispatch coord -> full-res GBuffer texel)
and, during its own post-report verification, discovered that
`FReSTIRPass.cpp` marshals these cbuffers **field-by-field into a flat
`float ConstantsData[64]`** rather than `memcpy`-ing the struct. It then
appended the three missing writes (`Pad[0]`, `Pad[1]`, `GBufferScale`).

That fix is correct **on the C++ side**. This cycle addresses the other half
of the same boundary, which v183 did not check: **whether the flat float
array the C++ writes actually lines up with the offsets HLSL assigns to the
shader-side struct.** For every field up to `PrevSceneYaw` it does. For the
three fields v183 just added, it does not.

## Root cause: HLSL packs array elements one-per-16-byte-register

This is a standard HLSL constant-buffer packing rule and it is the single
most common C++/HLSL cbuffer desync:

> Arrays in a constant buffer are **not** packed tightly. Each element of an
> array is forced to start on a new 4-component (16-byte) boundary.

So `float Pad[2]` does **not** occupy 2 consecutive floats. It occupies
2 *registers* = 8 floats, with the payload in slots 0 and 4.

Concretely, counting floats from the start of `FReSTIRTemporalConstants`
(`ReSTIR_Temporal_cs.hlsl:22-37`). The two 4x4 matrices are 32 floats and
are register-aligned, so scalar packing begins cleanly at float 32:

| field | C++ writes at (FReSTIRPass.cpp) | HLSL reads at | match |
|---|---|---|---|
| `OutputSize[2]`, `RcpOutputSize[2]` | 32..35 | 32..35 | ok |
| `FrameIndex` | 36 | 36 | ok |
| `MaxM` | 37 | 37 | ok |
| `DepthThreshold` | 38 | 38 | ok |
| `NormalThreshold` | 39 | 39 | ok |
| `DebugVis` | 40 | 40 | ok |
| `SceneYaw` | 41 | 41 | ok |
| `PrevSceneYaw` | 42 | 42 | ok |
| `Pad[0]` (near) | **43** | **44** (next 16B boundary) | **MISMATCH** |
| `Pad[1]` (far) | **44** | **48** (one register later) | **MISMATCH** |
| `GBufferScale` | **45** | **52** | **MISMATCH** |

`DebugVis`/`SceneYaw`/`PrevSceneYaw` fill floats 40,41,42 — i.e. registers
slots .x/.y/.z of register 10. `Pad[]` is an array, so it cannot start at
slot .w (float 43); it is pushed to float 44 (register 11.x). `Pad[1]` then
starts a further register on at float 48. `GBufferScale` follows the array
at float 52.

**Consequence with the current code:** the shader reads
`nearP = Pad[0]` from float 44 — where C++ actually wrote `Pad[1]` (50.0).
It reads `farP = Pad[1]` from float 48 — where C++ wrote nothing (0.0,
from the `memset`). And `GBufferScale` reads float 52 — also zero.

So today, post-v183:
- `nearP = 50.0`, `farP = 0.0` → `(farP - nearP)` is negative and the exact
  NDC-z reconstruction at `:144` produces a wrong value. This is *worse than
  the pre-v183 state*, where both were 0 and `ndcZ` stayed 0 via the
  `currDepth > 1e-6` guard path producing a consistent (if degenerate) value.
- `GBufferScale = 0` → `GB()`'s `max(int(0),1)` clamps to 1 → **the entire
  v183 half-res fix is inert.** v183's own audit predicted "M mean should
  rise substantially from 2.93"; with scale clamped to 1 it cannot, and that
  prediction would have been falsely recorded as a refutation of a correct
  hypothesis.

The spatial cbuffer is **not** affected: `FReSTIRSpatialConstants`
(`ReSTIR_Spatial_cs.hlsl:16-27`) is all scalars and `float2`s with
`GBufferScale` at float 9 and a trailing scalar `float Pad` — no array, so
C++ float 9 == HLSL float 9. v183's spatial half of the fix is sound.

## Approach

Remove the array. Replace `float Pad[2]` with two named scalars in both the
HLSL and the C++ mirror, and order them so the flat write sequence in
`FReSTIRPass.cpp` is already correct without touching the marshaller.

**`ReSTIR_Temporal_cs.hlsl:35-36`**
```hlsl
    float NearPlane;     // was Pad[0]
    float FarPlane;      // was Pad[1]
    float GBufferScale;
```
**`FReSTIRPass.h:44-45`** — same three scalars, same order.

**`FReSTIRPass.cpp:449-451`** — `Constants.Pad[0]` → `Constants.NearPlane`,
`Constants.Pad[1]` → `Constants.FarPlane`. Offsets 43/44/45 unchanged and
now correct, because three consecutive scalars pack tightly: 43 = reg 10.w,
44 = reg 11.x, 45 = reg 11.y.

**`ReSTIR_Temporal_cs.hlsl:140-141`** — read `gConstants.NearPlane` /
`gConstants.FarPlane`.

**`TestReSTIR_GI_Temporal.cpp:976-977`** — `TC.Pad[0]` → `TC.NearPlane`,
`TC.Pad[1]` → `TC.FarPlane`.

## Why scalars rather than `float4`/explicit `packoffset`

`packoffset` would work but adds a second source of truth that the next
field-append can desync again. Plain scalars pack tightly and predictably,
match the convention every other field in these two structs already uses,
and make the C++ order-equals-HLSL-order invariant hold by inspection —
which is exactly the property the field-by-field marshaller depends on.

## Sibling-test safety

`TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl:15` declares `float Pad[3]`
and **never reads it** (verified: the only `.Pad[` reads in the whole
Runtime tree are the two lines in the GI_Temporal copy). `TestCornellBoxGI`
shares `FReSTIRPass.cpp`, so it will now receive three extra floats at 43/44/45.
Those land inside its declared padding region and are never read. No change
in behaviour. Do not edit the Cornell copy.

## test_strategy (for role #5)

File-only static verification, since shell is unavailable:
1. Zero `Pad[` remains in `TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl`.
2. Field order in `FReSTIRPass.h` == field order in the HLSL struct, element
   by element, and == the write order in `FReSTIRPass.cpp:432-451`.
3. No array type remains in either the temporal or spatial cbuffer struct
   (matrices excepted — those are register-aligned and correct).
4. Both `.Pad[` consumers updated; no dangling reference anywhere in tree.
5. Cornell sibling untouched (0 diff).
6. `ReSTIR_Temporal_cs.hlsl` still listed in `ShaderMake.cfg` (it is, line 6).

## risks

- **This is a production-path change** (not `#ifdef`-confined). It can move
  the validator and the display image in either direction. Report honestly.
- Field-order parity between three files is the failure mode; the tester must
  check it element-by-element, not by size.
- The `float ConstantsData[64]` buffer must still cover the high-water mark:
  46 floats used of 64. Fine.
- **Falsifiable prediction:** with `GBufferScale` now actually arriving as 2.0
  (rather than 0 → clamped to 1), and `near`/`far` arriving as 0.001/50.0
  rather than 50.0/0.0, `ReSTIR summary: M mean` should rise substantially
  from `2.93` toward `MaxM=30`. If it does not, the half-res-mismatch
  hypothesis is wrong and must be recorded as a refutation, not rationalised.
