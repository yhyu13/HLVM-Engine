# Pending Plan Review v91
- plan: docs/PENDING_PLAN_v91.md
- verdict: KEEP
- reviewer: plan-criticer (v91)
- timestamp: 2026-07-28T23:NN

## Design soundness
v91 narrows v90's 2-way downstream-surface hypothesis to a 1-way hypothesis via a slot-validity probe that compares three sites: the binding-layout slot, the shader register, and the binding-set's first-arg slot. If all three are 0, the binding is consistent and the descriptor must reach the shader — eliminating hypothesis (ii) at the binding layer. Acceptable.

## Plan completeness
- Three NEW diagnostic sites distinct from v25-v90:
  - B1: `UAVItems[0].slot = 0` at FGIPass.cpp:304 (binding layout declaration)
  - B2: `RWTexture2D<float4> Output : register(u0);` at GIPathTracing.hlsl:88 (shader register)
  - B3: `UAVBuilder.SetTextureUAV(0, Desc.OutputTexture)` at FGIPass.cpp:582 (binding-set population)
- The plan correctly identifies the risks (HLSL register vs SPIR-V translation; nvrhi slot first-arg semantics; keepInitialState on UAV).
- 0 source-code lines; mirrors v89/v90's diagnostic-only cycle shape.
- Follows gpu-rendering-bisect-debug skill's "one variable per experiment" rule.

## Plan-fidelity check
KEEP. Matches v89 PARTIAL_KEEP_BINDING_NARROW + v90 PARTIAL_KEEP_NARROWED lineage. v91 narrows 2-way → 1-way; if all three slots are 0, the only remaining cause is (i) dispatch-drops, which requires terminal evidence per PIPELINE_BLOCKER. If any slot differs, the bug is binding-side (a different fix-branch, e.g. slot rewrite) and the next cycle becomes a FIX cycle on the binding.

## What v91 would NOT do (consistent with v25-v90 pattern)
- Would NOT rebuild, run, validate, or vision-analyze (terminal blocked).
- Would NOT bump ambient/lighting constants; aa2cc53 already fixed those.
- Would NOT re-add WriteGBufferSentinels; e6b3d52 correctly removed them.
- Would NOT touch worldpos normalization; 2fab7d6 already fixed it.
- Would NOT touch source code; 0 net lines by design.
