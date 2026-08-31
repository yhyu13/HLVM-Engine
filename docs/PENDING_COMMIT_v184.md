# Pending Commit v184

- plan: docs/PENDING_PLAN_v184.md
- plan_review: docs/PENDING_PLAN_REVIEW_v184.md (KEEP, no iteration)
- impler: agent_3_impler (tick-531)
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl,
  Engine/Source/Runtime/Public/Renderer/PostProcess/FReSTIRPass.h,
  Engine/Source/Runtime/Private/Renderer/PostProcess/FReSTIRPass.cpp,
  Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
- source: no bundle — direct edit
- target: (uncommitted working tree; this job does not commit or push)
- task: replace the temporal cbuffer's `float Pad[2]` with two plain scalars
  so C++ flat-float write offsets match HLSL register packing
- verify: ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
  && cd Engine/Source/Runtime/Binary/Debug
  && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
- skip_impl_review: no
- produces_test_files: no

## Change applied (5 sites, 4 files)

**`ReSTIR_Temporal_cs.hlsl`**
- `:35` `float Pad[2]` + `float GBufferScale`
  → `float NearPlane; float FarPlane; float GBufferScale;` (+ 5 comment lines
  recording the packing rule so the next field-append does not regress it).
- `:140-141` `gConstants.Pad[0]/[1]` → `gConstants.NearPlane/.FarPlane`.

**`FReSTIRPass.h`**
- `:44` `TFP32 Pad[2]` → `TFP32 NearPlane; TFP32 FarPlane;` — mirror order
  identical to the HLSL struct.

**`FReSTIRPass.cpp`**
- `:449-450` `Constants.Pad[0]/[1]` → `Constants.NearPlane/.FarPlane`.
  **Write offsets 43/44/45 are unchanged** — the fix is on the read side's
  packing, not the write side's ordering.

**`TestReSTIR_GI_Temporal.cpp`**
- `:976-977` `TC.Pad[0]/[1]` → `TC.NearPlane/.FarPlane` (values 0.001f/50.0f
  unchanged).

## What this actually fixes

Three floats were being read from the wrong registers:

| field | C++ wrote float | HLSL read float (before) | now |
|---|---|---|---|
| near  | 43 | 44 → got 50.0 (the far value) | 43 ✓ |
| far   | 44 | 48 → got 0.0 (memset)         | 44 ✓ |
| GBufferScale | 45 | 52 → got 0.0 (memset)  | 45 ✓ |

Cause: HLSL forces each constant-buffer **array** element onto a fresh
16-byte register. `float Pad[2]` therefore could not start at float 43
(register 10 slot .w) and was pushed to 44, with `Pad[1]` at 48 and
`GBufferScale` displaced to 52. Three consecutive scalars pack tightly at
43 (reg 10.w), 44 (reg 11.x), 45 (reg 11.y), matching the C++ writes exactly.

**Most significant consequence:** `GBufferScale` was arriving as 0, and both
`GB()` helpers clamp via `max(int(scale), 1)`. The temporal half of the v183
half-res fix was therefore **inert** — it compiled, looked correct, and
changed nothing. The spatial half was live (that struct has no array;
`GBufferScale` is float 9 on both sides), so v183 was half-working.

## Verification performed (static only)

- `search_files pattern="\.Pad\["` over `Engine/Source/Runtime` → **0 hits**.
  No dangling reference anywhere in the tree.
- Field order re-checked element-by-element across all three mirrors
  (`FReSTIRPass.h:21-51`, `ReSTIR_Temporal_cs.hlsl:22-43`,
  `FReSTIRPass.cpp:428-451`) — identical sequence, 46 floats high-water
  against `ConstantsData[64]`.
- `ShaderMake.cfg:6` still lists `ReSTIR_Temporal_cs.hlsl -T cs`, so the
  edited copy is the one that gets compiled.
- `TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl` untouched (0 edits).

## Plan Deviations

**None.** All five sites were exactly as the plan described; no obstacle
required deviating.

## Notes for the reviewer

1. **Check field-order parity element-by-element, not by size.** A rename
   that preserves size can still swap order — that is the one way this class
   of change breaks silently.
2. **Latent trap found in passing, deliberately NOT fixed (out of scope).**
   `FReSTIRPass.h:28` declares `TFP32 Pad[2]` in `FReSTIRConstants`
   (generation pass) while `ReSTIR_Generate_cs.hlsl:22` declares `float2 Pad`.
   These disagree in kind — but the field is **never marshalled** (the
   generation marshaller stops at `DebugVis`, `FReSTIRPass.cpp:363`) and
   **never read** by the shader, so it is currently harmless. It is a live
   trap for whoever next appends a field to that struct. Flagging rather than
   fixing, to keep this commit scoped; worth its own card.
3. **Sibling safety.** `TestCornellBoxGI` shares `FReSTIRPass.cpp` and now
   receives three floats at 43/44/45. Its shader declares `float Pad[3]` there
   and never reads it, so they land in unread padding. No behaviour change.
4. **No runtime verification was possible.** Every `terminal` call in this
   runspace — including a bare `date` — is denied by tirith with
   `status: pending_approval, pattern_key: tirith:unknown, exit_code: -1`.
   There is no vision tool in the enumerated toolset either. **No acceptance
   gate may be reported as PASS on the strength of this patch.**
5. **Production-path change.** Combined with v183, the working tree now
   carries two unverified production changes. The `M mean` prediction is
   falsifiable and must be reported honestly in either direction.
