# Pending Plan Review v1 — fix TestReSTIR_GI_Temporal GBuffer SRV binding

- plan: docs/PENDING_PLAN_v1.md
- verdict: KEEP
- reviewer: planner+plan-criticer (single-profile host; same head)
- timestamp: 2026-08-16

## Design soundness

The three-step probe+fix strategy correctly discriminates the two
remaining root-cause candidates:

1. **Compile-time macro mismatch** (`-D HLVM_RGI_DEBUG_VIS` missing from
   `create_restir_gi_temporal_shadermake` while cases 20/21/22 in the
   HLSL source gate on it). This is a definite bug; even if it isn't
   THE bug, fixing it ensures the diagnostic modes are reliably present
   in fresh builds.
2. **SPIR-V vs binding-layout binding-slot mismatch** (the shader
   compiles `register(t1)` to SPIR-V `Binding=1` if `--tRegShift=0` is
   used; the binding layout has slot=1 for t1; with `bindingOffsets.
   shaderResource=0` the descriptor goes to slot 1; this should match
   — but if any of these defaults are off by a shift, the binding is
   silently wrong). This needs an explicit reflection probe.

The plan also adds a `debugName`-based identity verification in
`FGIPass::DispatchRays` (small, gated by `HLVM_RGI_DIAG`), which is a
cheap defensive logging addition that doesn't change behavior.

## Plan completeness

The plan is missing one small explicit check: the **HLVM_RGI_DIAG
binding-layout dump** at FGIPass.cpp:670-685 already exists and emits
the binding layout items + set items with resourceHandle addresses.
The plan should call out that this dump will be the immediate
post-build verification — without rebuilding, just rerunning with
`HLVM_RGI_DIAG=1`.

## Risks acknowledged

- Single-profile host caveat: plan-criticer = planner in this run.
  Verdict weight is reduced.
- Build dependency: the macro change requires a full rebuild. If the
  build environment is missing `slangc` (per AGENTS.md "spdlog not
  found" environment fragility), the fix cannot be verified end-to-end
  in this tick.
- The "single-set reference" comparison (`TestCornellBoxGI`) works
  without the v22 split — this suggests the split itself might be the
  bug. Plan does not investigate this; if Probe 1+2 don't fix the bug,
  Plan v2 should revert the v22 split and compare.

## Feedback for planner (FIX only)

None — verdict is KEEP. Move to impl.

## Next role

Impler — implement the macro fix in `Engine/Source/Runtime/ShaderMakeBuild.py`
and add the identity verification logging in `FGIPass.cpp`.