# Pending Commit v28

- plan: docs/PENDING_PLAN_v28.md
- files: Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl, Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl
- source: no bundle
- target: (no commit; per cron instruction "do not commit/push/rewrite history")
- task: extend diagnostic surface with unconditional alpha-channel alive-sentinel for default-mode runs
- verify: `diff -u Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` should be empty (byte-identical after sync)
- skip_impl_review: no — even small diagnostic-surface patches follow the marker discipline
- produces_test_files: no
- notes: this is a diagnostic-surface expansion (+12 / -0 lines per HLSL copy, byte-identical); 0 source-code logic changes outside HLSL. Per cron instruction, no commit is performed; the patch is on disk awaiting parent rebuild.

## Patch summary
- Added 11 lines (10 comment + 1 alive-sentinel write) AFTER `Output[pixel] = float4(debugColor, avgFirstHitDist);` at line 682 in BOTH HLSL copies.
- Sentinel: `Output[pixel].w = max(Output[pixel].w, 0.99994f);`
- Purpose: independent of `debugMode` value, fires on every pixel that reaches the write — gives parent a definitive "yes the dispatch body ran" signal on default mode-0 runs.

## Plan Deviations (impler fills this in if it deviated)

None — impler followed plan exactly. +12 lines per file (vs plan's +5 estimate); comment-block formatting adds 7 lines beyond the plan's 4-comment estimate, but the structural shape is identical: 1 sentinel write + comment explanation. The deviation is cosmetic, not functional.

## What this commit does NOT do
- Does NOT fix the renderer (diagnostic-surface expansion only).
- Does NOT modify any C++ file.
- Does NOT introduce new debug-mode cases (uses existing Output[pixel] write path).
- Does NOT commit, push, or rewrite history (per cron instruction).
- Does NOT replace parent-driven terminal verification.