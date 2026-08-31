# Pending Commit v151
- plan: docs/PENDING_PLAN_v150.md
- files: Engine/Source/Runtime/Private/Renderer/PostProcess/FReSTIRPass.cpp, Engine/Source/Runtime/Public/Renderer/PostProcess/FReSTIRPass.h, Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ReSTIR_Generate_cs.hlsl, Engine/Source/Runtime/Test/TestCornellBoxGI_Data/ReSTIR_Generate_cs.hlsl
- source: no bundle — direct edit
- target: working tree
- task: Split FReSTIRPass::GenerationLayout into SRV-only (set 0) + UAV-only (set 1) per the bug-075 pattern that already fixed TemporalLayout, and add space1 to the generate shader's UAV declarations so SPIR-V reflection places them in set 1 to match the C++ split.
- verify: ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
- skip_impl_review: no
- produces_test_files: no
- notes: The temporal layout was split in bug-075 (six-role-pipeline v1) but the generation layout was overlooked; this v151 patch mirrors the split onto the generation pipeline. The mixed SRV+UAV single layout was the only ReSTIR layout that still combined both, so this is the last binding-layout split needed for the bug-075 fix family. Both ReSTIR_Generate_cs.hlsl copies (TestReSTIR_GI_Temporal_Data and TestCornellBoxGI_Data) are kept in sync.

## Plan Deviations
The v150 plan said "verify the Generate-pass binding layout is similarly split, and verify the Generate-pass's SRV reads of gi_raw / GBufferWorldPos / GBufferNormal actually see the data the GI shader wrote (not the sentinel that bit HLVM-Engine)." This v151 commit only performs the binding-layout split. Sentinel verification + a fresh non-bypass run require terminal access (EC-039 cumulative ≥1100 denials in this cron runspace). The "fresh log shows reservoir_radA/MW_A/radB/MW_B all zero" claim in PENDING_PICK.md line 4 was based on a misread of the 2026-08-05 15:42 log, which had HLVM_RGI_BYPASS=1 set — ReservoirTex0/1 are never written in bypass mode, so the all-zero stats are the expected behavior of bypass mode, not evidence of a ReSTIR defect. The actual non-bypass ReSTIR runtime path has not been exercised in any saved log on disk; this v151 source change is a candidate fix that needs the operator runspace to build, run, dump, validate, and vision-verify per the acceptance criteria.
