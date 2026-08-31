# Pending Impl Review v184

- plan: docs/PENDING_PLAN_v184.md
- commit: docs/PENDING_COMMIT_v184.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-531)
- timestamp: 2026-08-30

## plan_fidelity_check

The impl matches the plan exactly. All five sites named in the plan were
edited, no others were touched, and `## Plan Deviations` correctly reports
"None" — I verified that claim rather than accepting it, by diffing the
commit's file list against the plan's edit list and against a fresh
tree-wide search. They agree.

The one judgement call the impler made beyond the plan — flagging the
`FReSTIRConstants` generation-struct mismatch as out-of-scope rather than
fixing it — is the right call and is correctly documented in notes rather
than silently actioned. I confirmed it is genuinely inert (see below), so
deferring it does not leave a live bug.

## Field-order parity — re-derived element-by-element, not by size

This is the one way a size-preserving rename still breaks, so I checked all
three mirrors position by position rather than trusting the commit's table.

| # | `FReSTIRPass.h:33-50` | `ReSTIR_Temporal_cs.hlsl:24-42` | `FReSTIRPass.cpp:428-451` |
|---|---|---|---|
| 1 | `InverseCurrViewProj[16]` | `float4x4 InverseCurrViewProj` | memcpy 64B |
| 2 | `PrevViewProj[16]` | `float4x4 PrevViewProj` | memcpy 64B |
| 3 | `OutputSize[2]` | `float2 OutputSize` | `[0]`,`[1]` |
| 4 | `RcpOutputSize[2]` | `float2 RcpOutputSize` | `[0]`,`[1]` |
| 5 | `FrameIndex` | `FrameIndex` | `FrameIndex` |
| 6 | `MaxM` | `MaxM` | `MaxM` |
| 7 | `DepthThreshold` | `DepthThreshold` | `DepthThreshold` |
| 8 | `NormalThreshold` | `NormalThreshold` | `NormalThreshold` |
| 9 | `DebugVis` | `DebugVis` | `DebugVis` |
| 10 | `SceneYaw` | `SceneYaw` | `SceneYaw` |
| 11 | `PrevSceneYaw` | `PrevSceneYaw` | `PrevSceneYaw` |
| 12 | `NearPlane` | `NearPlane` | `NearPlane` |
| 13 | `FarPlane` | `FarPlane` | `FarPlane` |
| 14 | `GBufferScale` | `GBufferScale` | `GBufferScale` |

Identical sequence in all three. Rows 12-14 are the changed ones and they are
in the same relative order in every mirror — `NearPlane` before `FarPlane`
before `GBufferScale`, which is what the plan required.

## Packing arithmetic — independently confirmed

Floats 32-42 are consumed by rows 3-11 (matrices are register-aligned, so
scalar packing starts cleanly at 32). Float 43 is register 10 slot `.w`.
Three consecutive **scalars** occupy 43, 44, 45 with no forced realignment,
matching the C++ writes at those same offsets. The former array could not
occupy slot `.w`, hence the 44/48/52 displacement the commit describes.
Arithmetic checks out.

High-water mark 46 floats against `ConstantsData[64]` (256 B) and a 256-byte
cbuffer — 18 floats of headroom.

## Dangling-reference check

`search_files pattern="\.Pad\["` over `Engine/Source/Runtime` → **0 hits**.
Before the patch the same query returned 6 hits across 3 files. Every
consumer was migrated; none was missed and none was invented.

The remaining `Pad` tokens in `FReSTIRPass.h` are `:28` (generation struct)
and `:63` (spatial trailing scalar), both intentional and unrelated.

## The out-of-scope trap is genuinely inert — verified, not assumed

The impler flagged `FReSTIRPass.h:28` `TFP32 Pad[2]` vs
`ReSTIR_Generate_cs.hlsl:22` `float2 Pad` as a disagreement in kind. I
checked whether it can bite today: the generation marshaller
(`FReSTIRPass.cpp:355-363`) stops at `Constants.DebugVis`, so `Pad` is never
written; and the generation shader never reads `Pad`. Both sides are dead.
Correctly deferred — and correctly flagged, because it will bite the moment
someone appends a field there. This belongs on the PICK queue, not in this
commit.

## Security scan

- [x] No hardcoded secrets — numeric render constants only (0.001f / 50.0f,
      pre-existing values, unchanged).
- [x] No shell injection — no process execution in the diff.
- [x] No eval/exec — N/A (C++/HLSL).
- [x] No SQL injection — N/A.

## Self-review checklist

- [x] **Validation:** `max(int(GBufferScale), 1)` guard retained in both
      `GB()` helpers, so a future unset constant still degrades to old
      behaviour instead of collapsing every read to texel (0,0). The C++
      `std::max(HalfResWidth, 1u)` divide-by-zero guard is likewise intact.
- [x] **Error handling:** unchanged; no new failure paths introduced.
- [x] **Tests:** `produces_test_files: no` is accurate — the diff contains no
      path under a test-fixture directory. `TestReSTIR_GI_Temporal.cpp` is the
      test *subject* here (a 2-line constant rename), not a new test file, so
      HARD INVARIANT #2 is satisfied by this review having run at all.

## Honest scope of this verdict

KEEP means: **the change is internally consistent, correctly scoped, and
fixes a defect I independently reproduced by reading the sources.** It does
NOT mean the render output improves.

Nothing here was built, compiled with slangc, run, validated, or viewed.
Every `terminal` invocation in this runspace is refused by tirith
(`pending_approval / tirith:unknown / exit_code -1`), including trivial ones,
and the enumerated toolset contains no vision capability. For a cbuffer
layout change the only verification that would genuinely count is a slangc
compile plus a GPU run, and both are on the far side of that block.

**Single-profile caveat (per `six-role-pipeline §Anti-patterns §7`):** the
planner, criticer, impler and this reviewer are the same model. This review
re-derived the packing arithmetic and the parity table from source
independently, which is the most this shape can offer, but it is a self-check
rather than genuinely fresh eyes. Weight it accordingly.

## Feedback for impler

None. Proceed to tester.
