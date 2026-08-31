# Pending Plan Review v2 — revert v22 split in FGIPass

- plan: docs/PENDING_PLAN_v2.md
- verdict: KEEP
- reviewer: planner+plan-criticer (single-profile host; same head)
- timestamp: 2026-08-16

## Design soundness

The revert-v22-split hypothesis is well-grounded:

1. **TestCornellBoxGI uses the SAME shader bindings (t1/t2/t3 at register
   space 0) without the v22 split and works.** This is the strongest
   possible control: identical shader resource declarations, different
   binding-layout topology.
2. **The v22 split was added to silence a validation warning, not to fix
   a correctness bug.** Reverting it is the safest possible experiment:
   if it doesn't fix the SRV reads, the warning returns (acceptable).
   If it DOES fix them, the SRV reads work and the warning is gone too.
3. **The HLSL changes are mechanical** (`register(u0, space1)` →
   `register(u0)`) and the C++ changes are large but mechanical
   (delete one layout + builder, merge UAV items into the primary).
4. **No new test files needed** — the existing modes 20/21/22/30/31 are
   the regression test. If they return non-zero, the fix worked.

## Plan completeness

The plan correctly identifies the HLSL change as a 2-line edit per
resource, but doesn't explicitly list **all four files** that need
edits. Let me enumerate:

1. `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl`
   — the canonical source. (Used by `TestPathTraceGI`, `TestPathTraceTriangle`,
   etc. that don't use v22 split already.)
2. `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl`
   — the test-data-dir copy that's actually compiled by
   `create_restir_gi_temporal_shadermake`. Must stay in sync.
3. `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` — C++ binding
   layout/set builder changes.
4. (Implicit) `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`
   may need to drop the `OutputDirection` field from `Desc` if the
   shader no longer writes it, but only if the shader is also changed
   to remove the write. Actually, the shader still writes it; only the
   register qualifier changes. So this file is NOT affected.

The plan should call out (2) explicitly. The impler must remember to
keep the two HLSL copies in sync — a one-line change in one but not
the other would cause a silent SPIR-V vs C++ binding-layout
mismatch.

## Risks acknowledged

1. **TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl sync drift.** This
   is the silent-divergence trap flagged in `create_restir_gi_temporal_shadermake`'s
   comment at ShaderMakeBuild.py:613-619 ("ninja dependency graph points
   at a different file than the compiler reads"). Both files need
   editing AND the build system should be configured to detect the
   drift.
2. **Build environment dependencies.** The shader change requires a
   slangc recompile. Per AGENTS.md, this host may lack `slangc` or have
   a stale ShaderMake binary. The parent must verify before triggering.
3. **Removing `CmdList->commitBarriers()` between binding-set creation
   and dispatch (line 792) might re-introduce the original nvrhi-
   deferred-barrier-ordering issue.** The original commit added this
   line explicitly. Reverting it should be a deliberate step with
   diagnostic logging to verify the binding still works.

## Feedback for planner (FIX only)

None — verdict is KEEP. Plan v2 is approved. Move to impl.

## Next role

Impler — apply the 3-file edit (HLSL × 2 + C++ × 1).