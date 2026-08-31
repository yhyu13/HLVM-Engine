# Pending Commit v159
- plan: docs/PENDING_PLAN_v159.md
- files: (none — no source change)
- source: no bundle — verification-only cycle per v159 plan
- target: working tree
- task: Cycle-stop re-affirmation with the next on-disk bisect signal (case-label liveness check via spirv-cross --reflect)
- verify: (operator) `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` then `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal` then `HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal` then `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` on the newest dump group and vision-check the display PNG and confirm mode-20 returns non-zero GBufferMaterial; AND `spirv-cross --reflect Engine/Source/Runtime/Binary/Debug/shaders/GIPathTracing.spv | grep -iE 'case|OpSwitch|OpSelectionMerge'` to confirm the case labels survived slangc dead-stripping.
- skip_impl_review: yes — this is a non-impl commit (no source change; cycle-stop re-affirmation only).
- produces_test_files: no
- notes: This commit does NOT advance the cycle to the tester role. Per the `six-role-pipeline` skill's anti-pattern #6 ("the 6-role pipeline is wrong for this work when the diagnosis is a single suspicious dump that needs a 5-min bisect"), spawning the tester + testing-verifier subagents would produce phantom verdicts (they cannot run `validate_restir_gi.py` or the test binary from this file-only runspace). The state machine is halted at this v159 marker awaiting a parent runspace with terminal+vision+python3+numpy+spirv-cross to perform the 7 acceptance checks.

## Source-side fix re-verification this tick (additional v159 evidence)
Read-direct verification of all source-side fixes on disk today (read-only via `read_file`/`search_files`; no terminal, no edits). All 4 fix anchors remain INTACT:
- `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:289` — `Builder.SetBindingOffsets(0, 0, 0, 0)` (v137)
- `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:264-269` — v22 split comment block confirming UAVBindingLayout creation separately
- `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:458-460,474` — v140 AmbientColor override (`const float* AmbientColorPtr = Desc.AmbientColor;` and `std::memcpy(Data.AmbientColor, AmbientColorPtr, ...)`)
- `Engine/Source/Runtime/Private/Renderer/PostProcess/FReSTIRPass.cpp:147-181` — v151 split (GenerationLayoutSRV + GenerationLayoutUAV)

## New this tick: case-label liveness check (narrows 2026-07-30 hypothesis #1)
The 2026-07-30 diagnostic listed 4 possible causes for the mode-20 zero result, ordered:
1. slangc dead-strip on the debug-mode switch
2. Image layout transition wrong
3. nvrhi silently dropping the second binding set
4. The actual GBuffer textures in `Desc.GBufferWorldPos/Normal/Material` are different from the ones the rasterizer wrote to (handles stale/mismatched)

The v158 handle-identity check FALSIFIED hypothesis #4. The v159 case-label liveness check is the next narrowing: if `spirv-cross --reflect` shows `OpSwitch` and case labels for modes 6, 7, 8, 9, 10, 11, 12, 20, 21, 22, 30, 31 in GIPathTracing.spv, hypothesis #1 is FALSIFIED. If the switch is collapsed to a single conditional on `g_GI.Params5.x == 0` (or if any case label is missing), hypothesis #1 is CONFIRMED and the fix is a 1-line change to the switch: replace it with an if-else chain or add `[[dont_strip]]`-equivalent attributes. Neither check is runnable from this file-only runspace.

## Cycle-stop precedent honored
Per the >1100-tick EC-039 history, spawning the tester + testing-verifier roles would produce phantom verdicts. The right move is to halt at the v159 marker and wait for human/operator runspace access.
