# Pending Plan v188

- task: NEW card C (opened by tick-534) — declaration mismatch in Cornell's TEMPORAL ReSTIR cbuffer
- source: no bundle — direct edit
- approach: Align `TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl`'s
  `FReSTIRTemporalConstants` with the shared C++ header (`FReSTIRPass.h:40-60`)
  and the Temporal-test copy. **The card's prescribed fix is wrong** (see below):
  the Cornell copy is short by FIVE scalars, not three. Also assign the five
  fields explicitly at the Cornell call site, per the v187 precedent, so no
  named-but-zero field is left for a future `GB()` helper to launder.
- diff_estimate: +12 / -1 lines
- skip_plan_review: no
- test_strategy: file-only static verifier (role #5) — three-way declaration
  agreement (C++ header ↔ both HLSL copies), full float-offset walk against the
  `FReSTIRPass.cpp:428-456` marshaller, assignment coverage at both callers,
  compiled-path confirmation via `ShaderMake.cfg`, and a scope fence proving
  `TestReSTIR_GI_Temporal_Data/` is untouched.
- risks: see §Risks

## THE CARD IS WRONG — corrected at this gate

Card C states the fix is:

> replace `float Pad[3]` with `float NearPlane; float FarPlane; float GBufferScale;`

**Applying that verbatim would misalign the struct by two floats and is worse
than leaving it alone.** Re-derived from source this turn:

Shared C++ header `FReSTIRPass.h:40-60` — 12 members after the two mat4s:

```
OutputSize[2] RcpOutputSize[2] FrameIndex MaxM DepthThreshold
NormalThreshold DebugVis SceneYaw PrevSceneYaw NearPlane FarPlane GBufferScale
```

`TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl:22-43` declares all twelve
and agrees.

`TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl:4-16` declares:

```
OutputSize RcpOutputSize FrameIndex MaxM DepthThreshold
NormalThreshold DebugVis  float Pad[3];
```

It is missing **`SceneYaw` and `PrevSceneYaw`** as well — the Phase-C fields
(`FReSTIRPass.h:51-52`, marshalled unconditionally at `FReSTIRPass.cpp:441-442`).
Card C never mentions them; `search_files pattern="SceneYaw"` over
`Engine/Source/Runtime/Test` returns 6 hits, **none in any `TestCornellBoxGI`
file**. So the tail is short by five scalars, not three, and the card's
three-scalar replacement would place `NearPlane` at float 41 — the slot C++
writes `SceneYaw` into.

## Offset walk (both directions)

The marshaller (`FReSTIRPass.cpp:428-456`) writes a flat `float[64]`:
mat4s occupy 0-31, then

| float | C++ writes |
|---|---|
| 32,33 | `OutputSize[0..1]` |
| 34,35 | `RcpOutputSize[0..1]` |
| 36 | `FrameIndex` |
| 37 | `MaxM` |
| 38 | `DepthThreshold` |
| 39 | `NormalThreshold` |
| 40 | `DebugVis` |
| 41 | `SceneYaw` |
| 42 | `PrevSceneYaw` |
| 43 | `NearPlane` |
| 44 | `FarPlane` |
| 45 | `GBufferScale` |

**Cornell today.** `DebugVis` is float 40 = register 10, slot `.x`. The v184
rule: HLSL puts each constant-buffer *array* element on its own 16-byte
register, so `Pad[3]` cannot begin at 41 — `Pad[0]`→44, `Pad[1]`→48,
`Pad[2]`→52. Against C++ writes at 41-45, the sole overlap is float 44
(`FarPlane`) landing in `Pad[0]`. `search_files pattern="gConstants.Pad"` over
the whole Runtime tree → **0 hits**, so it is read nowhere. Inert today —
the card is right about that much.

**Cornell under the card's literal fix.** `NearPlane`→41, `FarPlane`→42,
`GBufferScale`→43. C++ puts `SceneYaw` at 41, `PrevSceneYaw` at 42,
`NearPlane` at 43. Every one of the three would be **named for the wrong
value** — the exact v184 failure mode, re-introduced by a patch whose stated
purpose is to prevent it. Not corrupting today (still unread), but it converts
a benign gap into three actively-misleading declarations.

**Cornell under this plan.** Five scalars replace `Pad[3]`: 41,42,43,44,45 —
name-for-name identical to the header and to the sibling copy. Scalars pack
tightly, so no register displacement occurs.

## The v187 coupling applies again

v187 established that adding a *named, readable* field backed by an unassigned
value is a regression even when nothing reads it yet, because a later
`GB()`-style helper with `max(int(s),1)` launders a garbage/zero scale into a
plausible wrong answer instead of an obvious one.

v187 already value-initialized `TempConstants` (`TestCornellBoxGI.cpp:1556`
`ReSTIR::FReSTIRTemporalConstants TempConstants{};`), so these five reads are
**defined zero**, not indeterminate — the dangerous half is gone. But
`GBufferScale = 0` is precisely the value v184 proved dangerous, and
`NearPlane = FarPlane = 0` would make any future `ndcZ` reconstruction divide
by `(farP - nearP) = 0`. So this plan assigns all five explicitly, values
derived from the call site, not guessed:

- `SceneYaw` / `PrevSceneYaw` = `0.0f` — Cornell has no scene rotation
  (0 hits for `SceneRotation` in the file); the sibling passes real degrees.
- `NearPlane` = `0.01f`, `FarPlane` = `10.0f` — read off Cornell's own
  projection at `TestCornellBoxGI.cpp:1276`
  `glm::perspectiveLH_ZO(glm::radians(90.0f), aspectRatio, 0.01f, 10.0f)`.
  (The sibling uses 0.001/50.0 at `TestReSTIR_GI_Temporal.cpp:998-999` —
  deliberately NOT copied; these are per-scene.)
- `GBufferScale` = `1.0f` — Cornell dispatches temporal at
  `CurrentFBInfo.width/height` (`:1585-1586`) and its `DepthTexture` /
  `NormalTexture` are the full-res GBuffer MRTs (`:1578-1579`), so the ratio
  is exactly 1. Same derivation v187 used for the spatial struct.

## Why the rendered image cannot move

Cornell's temporal shader reads `gConstants` in exactly 7 places
(`search_files` over that file): `:38 OutputSize`, `:64 RcpOutputSize`,
`:69 InverseCurrViewProj`, `:73 PrevViewProj`, `:101 DepthThreshold` +
`NormalThreshold`, `:131 FrameIndex`, `:142 MaxM`. **None of the five added
fields is read**, and this plan adds no read. Floats 0-40 are untouched in
both layout and value. So the patch is inert on the wire for every float the
shader consumes, and cannot change a Cornell pixel — the same standard v186
and v187 were held to.

Note this differs from v187 in one respect worth stating: v187 was byte-inert
(float 9 written either way). v188 is **not** byte-inert — floats 41,42,43,45
go from unwritten-into-a-hole to written-into-named-slots, and 44 moves from
`Pad[0]` to `FarPlane`. It is *read*-inert, which is the property that matters
for the image, and the stronger claim is deliberately not made.

## Files

- Modify: `Engine/Source/Runtime/Test/TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl:15`
- Modify: `Engine/Source/Runtime/Test/TestCornellBoxGI.cpp:1569` (append 5 assignments)

## Acceptance criteria (testable file-only)

1. Cornell temporal shader declares, in order after `DebugVis`: `SceneYaw`,
   `PrevSceneYaw`, `NearPlane`, `FarPlane`, `GBufferScale` — all plain scalars.
2. No `Pad` array remains in any `ReSTIR_*` shader in either data directory.
3. The three declarations (C++ header, Temporal-test copy, Cornell copy) agree
   name-for-name and kind-for-kind across all 12 post-matrix members.
4. All five new fields are explicitly assigned in `TestCornellBoxGI.cpp` before
   `DispatchTemporal`.
5. Cornell's temporal shader still contains no `GB(` helper and no read of any
   of the five fields.
6. `TestReSTIR_GI_Temporal_Data/` and `FReSTIRPass.{h,cpp}` are untouched, so
   the v183+v184+v185 chain awaiting its single operator run is not perturbed.
7. `TestCornellBoxGI_Data/ShaderMake.cfg` compiles the edited shader (v182 trap).

## Risks

- **Cannot be compiled or run here.** `terminal` is denied categorically by
  tirith — two probes this tick, including a bare `/usr/bin/true` with absolute
  path, no arguments, `workdir=/tmp`, both `pending_approval / tirith:unknown /
  exit_code -1`. slangc acceptance is UNVERIFIED.
- The struct grows from 41 to 45 declared floats. The buffer is `float[64]`
  (`FReSTIRPass.cpp`), so there is headroom, but only a build proves the
  cbuffer size nvrhi derives from reflection still matches.
- `0.01f/10.0f` are correct only while Cornell's projection at `:1276` is
  unchanged. Left as literals mirroring the sibling's style; a build is the
  only way to confirm they are never read anyway.
