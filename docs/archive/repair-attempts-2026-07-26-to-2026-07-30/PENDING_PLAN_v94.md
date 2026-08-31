# Pending Plan v94
- task: restir-gi-fix — file-only re-confirmation of v93 root cause + cron-posture escalation
- source: no bundle — direct read
- approach: (1) Re-verify on disk that v93's three file-only findings are STILL intact at v94 (no parent-driven source-code edits between v93 and v94); (2) acknowledge that this cron's actual runspace is file-only (terminal blocked by tirith with `pending_approval: tirith:unknown` — verified fresh this tick), so the 4-command recipe from `PIPELINE_BLOCKER_2026-07-28.md` cannot be run from this cron's session; (3) propose cron-posture change: stop looping on `restir-gi-fix` from this runspace until parent supplies terminal evidence; pivot PICK to parent-evidence-gated; (4) mark v93 ROOT_CAUSE_NAMED as the cron's deliverable. **NO new diagnostic finding** is offered this tick — v93 already exhausted the file-only diagnosis. v94's value is *closing* the loop, not advancing it.
- diff_estimate: +0/-0 source-code lines; +~30 lines across 6 PENDING_*_v94.md markers + HEALTH append + PICK update
- skip_plan_review: no
- test_strategy: testing-verifier re-confirms v93 P1+P2+P3 still on disk via read_file cross-tick verification
- risks: If a parent-driven source-code edit landed between v93 and v94 (e.g., parent applied the v93 fix recipe from a different terminal session), the v93 diagnosis may now be stale. The plan-stage spot-check below confirms or falsifies this risk. If the diagnosis is stale, v95 should pivot to a fresh diagnostic chain rather than continuing the v93 root-cause-named narrative.

## Plan-stage spot-check (file-only cross-tick verification)
This is the v94 file-only probe — verifying that v93's findings have not been invalidated by intervening source edits between v93 and v94.
- [x] P1.intact: `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl:88` still reads `RWTexture2D<float4> Output : register(u0);` (no space1) — verified this tick via search_files
- [x] P1b.intact: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl:88` still reads identically — verified this tick
- [x] P2.intact: `Engine/Source/Runtime/Private/Renderer/RayTracing/FRayTracingPipeline.cpp:149` still reads `PipelineDesc.globalBindingLayouts = { BindingLayout };` (no UAVBindingLayout push) — verified this tick
- [x] P3a.intact: `Engine/Source/Runtime/Private/Renderer/PostProcess/FReSTIRPass.cpp` still registers both SRV+UAV layouts and references space1 — verified via search_files
- [x] P3b.intact: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl:32-33` still declares `register(u0, space1)` / `register(u1, space1)` — verified via search_files
- [x] v28 alpha-sentinel intact: `GIPathTracing.hlsl:694` still reads `Output[pixel].w = max(Output[pixel].w, 0.99994f);` in BOTH Private+Data copies — verified this tick

All 6 v93 findings intact. The diagnosis is NOT stale.

## Cycle-meaning
v94 transition: v93 ROOT_CAUSE_NAMED → v94 RUNSPACE_BLOCKED (new semantic, distinct from v25-v93 PARTIAL_KEEP* / ALL_KEEP* / ROOT_CAUSE_NAMED variants). The cron's prompt this turn declares `enabled_toolsets: ["terminal","file"]`, but the cron's actual runspace is file-only (tirith blocks all `terminal` calls with `pending_approval: tirith:unknown` — verified 3+ fresh attempts this tick). The 6 acceptance criteria all require terminal execution; the cron's "do not silently stop" is satisfied by writing this marker set + HEALTH append + PICK pivot; the cron's "do not loop indefinitely" is satisfied by changing cron posture to parent-evidence-gated.