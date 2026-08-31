# Pending Commit v188

- plan: docs/PENDING_PLAN_v188.md
- files: Engine/Source/Runtime/Test/TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl, Engine/Source/Runtime/Test/TestCornellBoxGI.cpp
- source: no bundle — direct edit
- target: working tree (NOT committed — job instruction forbids commits)
- task: align Cornell's temporal ReSTIR cbuffer with the shared C++ header and assign the five previously-unwritten fields at the call site
- verify: `./Build.sh --Config=Debug --Target=TestCornellBoxGI --Rebuild --Test`
- skip_impl_review: no
- produces_test_files: no
- notes: see below

## What changed

**1. `TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl:15`** — `float Pad[3]`
replaced by five plain scalars `SceneYaw; PrevSceneYaw; NearPlane; FarPlane;
GBufferScale;`, matching `FReSTIRPass.h:51-59` and
`TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl:33-42` name-for-name and
kind-for-kind. +25 lines of which 20 are comment.

**2. `TestCornellBoxGI.cpp`, after `TempConstants.DebugVis = 0.0f`** — five
explicit assignments. +23 lines of which 18 are comment.

Net non-comment: +10 / -1.

## Verification performed here (file-only)

- Three-way agreement re-read after editing: header `FReSTIRPass.h:44-59`,
  sibling `:26-42`, Cornell `:8-38` — all twelve post-matrix members now agree
  in name, order and kind.
- `search_files pattern="float Pad\["` over `Engine/Source/Runtime` → **6 hits,
  not 0** (this claim was wrong as originally written and was corrected at the
  impl-review gate). One is the v188 comment in this very file; two are the
  v184/v183 explanatory comments in the sibling shader and `FReSTIRPass.cpp`.
  The three real declarations are `FGBufferFillPass.h:21 Pad[12]`,
  `FToneMappingPass.h:30 Pad[56]`, `FContactShadowsPass.h:18 Pad[2]` — all
  **trailing** members of `static_assert(sizeof(...) == 256)` structs that are
  `memcpy`'d whole (`FContactShadowsPass.cpp:152`), not marshalled
  field-by-field. Being trailing, they displace nothing ahead of them; being
  memcpy'd, C++ layout is what reaches the wire. Not the v184 bug class. The
  correct narrow claim is: **0 `Pad` arrays remain in any `ReSTIR_*` shader or
  in `FReSTIRPass.h`.**
- `search_files pattern="gConstants.Pad"` over `Engine/Source/Runtime` → 0 hits
  (was already 0; the removed field was read nowhere, which is why this cannot
  move a pixel).
- Cornell temporal shader's `gConstants` reads re-enumerated after the edit:
  7 hits at `:63/:89/:94/:98/:126/:156/:167`, none naming any of the five new
  fields. Read-inert confirmed post-edit, not just pre-edit.
- `TestCornellBoxGI_Data/ShaderMake.cfg:6` compiles `ReSTIR_Temporal_cs.hlsl` —
  the v182 "patched a copy nothing compiles" trap does not apply.
- Scope fence: no file under `TestReSTIR_GI_Temporal_Data/` and neither
  `FReSTIRPass.h` nor `FReSTIRPass.cpp` was touched, so the v183+v184+v185
  chain awaiting its operator run is unperturbed.

## Plan Deviations

**None in substance.** Two notes:

1. The plan estimated +12/-1. Actual is +48/-1 raw, +10/-1 excluding comment.
   The excess is the packing rationale the plan-criticer asked to be carried
   into the comment (the 16-byte straddle rule for a future appended vector).
   Recorded here rather than silently, because a 4x line-count overrun against
   the plan is exactly the kind of thing an impl-review should be able to see
   declared.

2. The plan carried "does the enlarged struct still fit the buffer?" as an open
   risk. The plan-criticer discharged it: `FReSTIRPass.cpp:317
   BufferDesc.byteSize = 256`, marshaller `float ConstantsData[64]` = 256 bytes,
   struct grows 41→45 floats = 180 bytes. 64 bytes headroom. I re-read `:317`
   and `:424` myself to confirm rather than inheriting the claim. No longer an
   open risk.

## What is NOT verified

No build, no shader compile, no run, no image. slangc acceptance of the widened
struct is the load-bearing unknown. `terminal` denied categorically by tirith
this tick (probes: compound `echo; date; pwd; git log`, and bare `/usr/bin/true`
with absolute path, no args, `workdir=/tmp` — both `status: pending_approval,
pattern_key: tirith:unknown, exit_code: -1, smart_denied: false`).
