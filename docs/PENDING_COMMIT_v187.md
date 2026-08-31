# Pending Commit v187

- plan: docs/PENDING_PLAN_v187.md
- files: Engine/Source/Runtime/Test/TestCornellBoxGI_Data/ReSTIR_Spatial_cs.hlsl, Engine/Source/Runtime/Test/TestCornellBoxGI.cpp
- source: no bundle — direct edit
- target: (working tree only — this pipeline does not commit)
- task: Align the Cornell spatial cbuffer with the shared C++ header, and stop the
  Cornell ReSTIR structs from feeding indeterminate memory to the marshaller.
- verify: `./Build.sh --Config=Debug --Target=TestCornellBoxGI --Rebuild --Test`
- skip_impl_review: no
- produces_test_files: no
- notes: see below. Both edits are required together; either alone is worse than
  neither (plan §"Why the naive fix would make the control WORSE").

## What changed

**1. `TestCornellBoxGI_Data/ReSTIR_Spatial_cs.hlsl:19-33`** — `float2 Pad;` →
`float GBufferScale; float Pad;` plus a comment recording why the field exists
and why this test must NOT grow a `GB()` helper without also going half-res.

Wire effect: **none.** `FReSTIRPass.cpp:547` already wrote float 9
unconditionally. `float2` is a vector (not an array), so it occupied floats 9/10
and the replacement scalars occupy 9/10. Byte-for-byte identical; only the name
float 9 carries changes.

**2. `TestCornellBoxGI.cpp:1513, 1556, 1607`** — added `{}` to all three ReSTIR
constant-struct declarations.

**3. `TestCornellBoxGI.cpp:1617-1623`** — `SpatConstants.GBufferScale = 1.0f;`
with the justification inline (this call site dispatches at `CurrentFBInfo`
resolution, `:1626-1627`, the same resolution as the GBuffer MRTs it samples).

## Why #2 is the substantive half

`FReSTIRPass.h:19-73` structs have no default member initializers, so
`ReSTIR::FReSTIRTemporalConstants TempConstants;` left every member
indeterminate. Cornell assigns a prefix and stops at `DebugVis`. The marshallers
then read past that prefix:

| Marshaller | Reads | Cornell assigns? | Indeterminate floats |
|---|---|---|---|
| Generation `FReSTIRPass.cpp:354-363` | stops at `DebugVis` (offset 8) | yes | 0 |
| Temporal `:454-456` | `NearPlane`, `FarPlane`, `GBufferScale` | **no** | 3 |
| Spatial `:547` | `GBufferScale` | **no** | 1 |

Four indeterminate floats per frame were being copied into a GPU-visible buffer
in the control test. The generation pad contributes zero despite being
unassigned, because that marshaller never reads it — counted precisely rather
than lumped in.

## Verification performed (file-only)

- `search_files pattern="FReSTIRSpatialConstants "` over Runtime → 5 hits; both
  C++ instantiations (`TestCornellBoxGI.cpp:1607`, `TestReSTIR_GI_Temporal.cpp:1042`)
  now carry `{}`.
- `search_files pattern="FReSTIRTemporalConstants "` → both instantiations
  (`:1556`, `:961`) carry `{}`.
- `search_files pattern="float2 Pad;"` over Runtime → 2 hits, both in
  `TestSponzaDeferred_Data` (`SSAOBlur_cs.hlsl:9`, `ExposureAdaptation_cs.hlsl:12`),
  neither a ReSTIR struct. No ReSTIR shader declares a vector pad any more.
- `TestCornellBoxGI_Data/ShaderMake.cfg:7` lists `ReSTIR_Spatial_cs.hlsl` — the
  edited shader is on a compiled path, not a dead copy (the v182 trap).
- `TestReSTIR_GI_Temporal_Data/` untouched → the v183+v184+v185 chain is
  unperturbed.

## Plan Deviations

**One, and it widened the C++ edit from the plan's letter.**

The plan named `TestCornellBoxGI.cpp:1513,1551,1602,1611` for the value-init. I
also checked the *other* test that shares these structs before editing, because
the v186 lesson was that `FReSTIRPass.h` is shared and patching one side
relocates a mismatch rather than removing it. `TestReSTIR_GI_Temporal.cpp`
already value-initializes all three (`:961`, `:1042`, and the generation site),
so no edit was needed there — but that check is the reason I can assert the
value-init is now uniform across both callers rather than assuming it.

No deviation in scope or intent. Line numbers shifted by the inserted comments.

## What is NOT established

No build, no shader compile, no run, no image. slangc acceptance of the edited
struct is **UNVERIFIED** — `terminal` is denied categorically by tirith (three
probes this tick, `pattern_key: tirith:unknown`, `exit_code: -1`). The edit is a
two-token change to a declaration whose exact post-patch form already compiles
in `TestReSTIR_GI_Temporal_Data/ReSTIR_Spatial_cs.hlsl:16-27`, which is the only
mitigation available here and is not a substitute for compiling it.

## Follow-up left open deliberately

Cornell's **temporal** shader declares `float Pad[3]` (`ReSTIR_Temporal_cs.hlsl:15`)
against C++ `NearPlane/FarPlane/GBufferScale`. HLSL places array elements at
44/48/52 while C++ writes 43/44/45, so only float 44 overlaps, at unread `Pad[0]`.
Value-init makes those writes defined; the declaration mismatch is a separate
card — different struct, and bundling would break one-variable-per-experiment.
