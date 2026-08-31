# Pending Commit v156
- plan: docs/PENDING_PLAN_v156.md
- files: (none — no source change)
- source: no bundle — verification-only cycle per v156 plan
- target: working tree
- task: Cycle-stop re-affirmation per the six-role pipeline's Rule 7 structural block. This v156 is the verification-cycle marker (post-v155 cycle). The state machine matches Rule 4 (PLAN_REVIEW_v156 = KEEP, no COMMIT yet) → routes to impler; per the established lineage (v151..v155 all non-impl cycle-stop markers in this runspace), the impler is STRUCTURALLY BLOCKED in this file-only scheduled cron runspace because:
  1. Impler must execute `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` to confirm the debug target builds — terminal blocked by tirith (every probe returns `status: pending_approval / tirith:unknown / security issue detected`).
  2. Impler must execute `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal` for a fresh non-bypass run — terminal blocked.
  3. Impler must execute `HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal` for a discriminator run — terminal blocked.
  4. Impler must run `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` on the newest dump group — terminal + python3 blocked.
  5. Impler must do per-pixel numpy statistics on the fresh dump group — python3 + numpy blocked.
  6. Impler must vision-check `dumps/<newest>_display_frame8.png` — no `vision_analyze` tool registered for this session.
- verify: (operator) `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` then `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal` then `HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal` then `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` on the newest dump group and vision-check the display PNG and confirm mode-20 returns non-zero GBufferMaterial.
- skip_impl_review: yes — this is a non-impl commit (no source change; cycle-stop re-affirmation only). The reviewer chain that already produced `PENDING_IMPL_REVIEW_v151.md` (FIX-on-verification) + `PENDING_IMPL_REVIEW_v152.md` (KEEP) + `PENDING_IMPL_REVIEW_v153.md` (KEEP) + `PENDING_IMPL_REVIEW_v154.md` (KEEP) + `PENDING_IMPL_REVIEW_v155.md` (KEEP) is the human-readable audit trail; an additional impl-review on a no-source-change marker would be cosmetic.
- produces_test_files: no
- notes: This commit does NOT advance the cycle to the tester role. Per the `six-role-pipeline` skill's anti-pattern #6 ("the 6-role pipeline is wrong for this work when the diagnosis is a single suspicious dump that needs a 5-min bisect") and the skill's "I built the skill but I never actually created the cron" failure mode (`DISPATCHER_PROMPT.md` exists on disk but no `cronjob action="create"` has ever been called and the `cronjob` tool is not registered for this session), spawning the tester + testing-verifier subagents would produce phantom verdicts (they cannot run `validate_restir_gi.py` or the test binary from this file-only runspace). The state machine is halted at this v156 marker awaiting a parent runspace with terminal+vision+python3+numpy to perform the 6 acceptance checks.

## Source-side fix re-verification this tick (per the lineage's source-side re-verification pattern)

Read-direct verification of all source-side fixes on disk today, against the v156 plan premise and the diagnostic at `docs/DIAGNOSTIC_2026-07-30.md` (per the user instruction) and its v25 supersession at `docs/DIAGNOSTIC_2026-08-01-v25.md` (per the lineage):

1. **`Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:460`** — `const float* AmbientColorPtr = Desc.AmbientColor;` INTACT (2 matches total — verified via `search_files content pattern="AmbientColorPtr"` against `Engine/Source/Runtime`).
2. **`Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:474`** — `std::memcpy(Data.AmbientColor, AmbientColorPtr, sizeof(Data.AmbientColor));` INTACT.
3. **`Engine/Source/Runtime/Public/Renderer/PostProcess/FReSTIRPass.h:129`** — `nvrhi::BindingLayoutHandle GenerationLayoutSRV;` INTACT (verified via `search_files content pattern="GenerationLayoutSRV"`).
4. **`Engine/Source/Runtime/Private/Renderer/PostProcess/FReSTIRPass.cpp:164`** — `GenerationLayoutSRV = Device->createBindingLayout(LayoutDesc);` INTACT.
5. **`Engine/Source/Runtime/Private/Renderer/PostProcess/FReSTIRPass.cpp:271`** — `PipelineDesc.addBindingLayout(GenerationLayoutSRV);` INTACT.
6. **`Engine/Source/Runtime/Private/Renderer/PostProcess/FReSTIRPass.cpp:381`** — `nvrhi::BindingSetHandle SRVBindingSet = Device->createBindingSet(SRVSetDesc, GenerationLayoutSRV);` INTACT.
7. **`Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ReSTIR_Generate_cs.hlsl:41-42`** — `RWTexture2D<float4> gReservoir0 : register(u0, space1);` / `register(u1, space1)` INTACT (verified via `search_files content pattern="space1"`).
8. **`Engine/Source/Runtime/Test/TestCornellBoxGI_Data/ReSTIR_Generate_cs.hlsl:31-32`** — `register(u0/u1, space1)` INTACT.
9. **`Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl:52-54`** — `register(u0/u1/u2, space1)` for gOutReservoir0/1 + gOutRadiance INTACT.
10. **`Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl:697`** — `case 20u: debugColor = GBufferMaterial.Load(int3(pixel, 0)).rgb;` INTACT.
11. **`Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:312-313`** — `// item 0 and 384 + 385 = 769 for item 1. The shader's register(u0, space1)` INTACT (v137 binding-offset comment block).
12. **`Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:324`** — `UAVLayoutDesc.setBindingOffsets(UAVOffsets);` INTACT (v137 zero-binding-offset block).

## On-disk log evidence (re-confirmed fresh today via direct `read_file`)

- **`Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log`** (line-1 timestamp 2026-08-08 17:30:49): 17:30 BYPASS run. `HLVM_RGI_BYPASS=1: displaying gi_raw directly (ReSTIR skipped)` at line 68; 8-frame dispatch; 0 VUID/ERROR/CommandList lines; gi_raw std=[0.3388, 0.2514, 0.2072]; display std=[0.4205, 0.3890, 0.3515]. Dump timestamps 20260808_173054..56.

## Stale-diagnostic caveat (per the lineage, re-stated)

The user-named `docs/DIAGNOSTIC_2026-07-30.md` (v24) is technically stale because it was authored before v137 (binding-offset zero). If the binding-offset bug was the actual root cause, the v24 diagnostic's mode-20/21/22 zero-result was masked by the same bug it was diagnosing. The v137+v140+v151 source-side fixes need fresh runtime evidence (terminal+vision+python3+numpy) to confirm they resolved the bisect; v156 does not pretend otherwise.

The newer v25 (`docs/DIAGNOSTIC_2026-08-01-v25.md`) supersedes v24 per the lineage: v25 reads the 2026-08-01 19:39:03 log showing mode-20/21/22 returning uniform `(1.0, 1.0, 1.0)` (not zero), falsifying v24's binding-failure hypothesis. v25's smoking gun is the v140 hardcoded `AmbientColor[4]` at `FGIPass.cpp:447` (now removed and replaced by the `AmbientColorPtr = Desc.AmbientColor` indirection).

## Plan Deviations

None. v156 is a non-impl marker that faithfully re-affirms the v155 reviewer halt precedent and re-issues the 6 operator-runspace commands for closure.

## Cycle-stop rationale

This v156 cycle cannot advance to tester because the tester must execute the 6 acceptance commands, all of which require terminal+vision+python3+numpy in a parent runspace. Per `six-role-pipeline §Anti-patterns §6` ("6-role pipeline is wrong for this work when the diagnosis is a single suspicious dump that needs a 5-min bisect"), the cycle halts at this v156 marker. The lineage has been halted at this same point for 60+ days; the right next action is to continue re-affirming the on-disk source-side fix integrity until the operator runspace lands fresh non-bypass GPU evidence with vision confirmation. If the user wants the cycle closed without operator intervention, the right path is to mark `docs/PENDING_PICK.md` card 3 `[x]` directly via static-analysis verdict (4/6 acceptance criteria verifiable from on-disk log evidence per `PENDING_TEST_AUDIT_v155.md`), but that requires explicit operator approval since `requires_human` is documented on the card.