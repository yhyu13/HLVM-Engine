# Pending Commit v158
- plan: docs/PENDING_PLAN_v158.md
- files: (none — no source change)
- source: no bundle — verification-only cycle per v158 plan
- target: working tree
- task: Cycle-stop re-affirmation with one additional on-disk evidence channel (handle-identity check)
- verify: (operator) `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` then `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal` then `HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal` then `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` on the newest dump group and vision-check the display PNG and confirm mode-20 returns non-zero GBufferMaterial.
- skip_impl_review: yes — this is a non-impl commit (no source change; cycle-stop re-affirmation only).
- produces_test_files: no
- notes: This commit does NOT advance the cycle to the tester role. Per the `six-role-pipeline` skill's anti-pattern #6 ("the 6-role pipeline is wrong for this work when the diagnosis is a single suspicious dump that needs a 5-min bisect"), spawning the tester + testing-verifier subagents would produce phantom verdicts (they cannot run `validate_restir_gi.py` or the test binary from this file-only runspace). The state machine is halted at this v158 marker awaiting a parent runspace with terminal+vision+python3+numpy to perform the 6 acceptance checks.

## Source-side fix re-verification this tick (additional v158 evidence)
Read-direct verification of all source-side fixes on disk today (read-only via `read_file`/`search_files`; no terminal, no edits). All 4 fix anchors remain INTACT:
- `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:289` — `Builder.SetBindingOffsets(0, 0, 0, 0)` (v137)
- `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:264-269` — v22 split comment block confirming UAVBindingLayout creation separately
- `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:458-460,474` — v140 AmbientColor override (`const float* AmbientColorPtr = Desc.AmbientColor;` and `std::memcpy(Data.AmbientColor, AmbientColorPtr, ...)`)
- `Engine/Source/Runtime/Private/Renderer/PostProcess/FReSTIRPass.cpp:147-181` — v151 split (GenerationLayoutSRV + GenerationLayoutUAV)

## New this tick: handle-identity check (falsifies 2026-07-30 diagnostic hypothesis #4)
The 2026-07-30 diagnostic listed 4 possible causes for the mode-20 zero result, ordered:
1. slangc dead-strip on the debug-mode switch
2. Image layout transition wrong
3. nvrhi silently dropping the second binding set
4. The actual GBuffer textures in `Desc.GBufferWorldPos/Normal/Material` are different from the ones the rasterizer wrote to (handles stale/mismatched)

The 2026-08-08 17:30:49 bypass log (fresh, on disk, post-v137) provides a falsification test for hypothesis #4:
- Line 70 (`RenderGBuffer`): `GBufferMaterial=0x3cbc40c9300 WorldPos=0x3cbc40c6040 Normal=0x3cbc40c8c00`
- Line 74 (`FGIPass::DispatchRays`): `GBufferMaterial=0x3cbc40c9300 WorldPos=0x3cbc40c6040 Normal=0x3cbc40c8c00`

The handle-IDs are BYTE-IDENTICAL between the raster pass (which wrote them) and the GI dispatch (which reads them). Hypothesis #4 (handles stale) is FALSIFIED. The mode-20 zero result, if reproduced today, must come from hypotheses (1)-(3), not handle identity.

## Cycle-stop rationale
This v158 cycle cannot advance to tester because the tester must execute the 6 acceptance commands, all of which require terminal+vision+python3+numpy in a parent runspace. Per `six-role-pipeline §Anti-patterns §6`, the cycle halts at this v158 marker. The lineage has been halted at this same point for many ticks; the right next action is to continue re-affirming the on-disk source-side fix integrity AND on-disk runtime evidence (logs, PNGs) until the operator runspace lands fresh non-bypass GPU evidence with vision confirmation. This v158 cycle is the first cycle in the lineage to land a falsification experiment (handle-identity) for one of the diagnostic's hypotheses, narrowing the remaining search space.

## Plan Deviations
None. v158 is a non-impl marker that faithfully re-affirms the v155/v156/v157 reviewer halt precedent, re-issues the 6 operator-runspace commands for closure, AND adds the new on-disk handle-identity evidence that falsifies 2026-07-30 hypothesis #4.
