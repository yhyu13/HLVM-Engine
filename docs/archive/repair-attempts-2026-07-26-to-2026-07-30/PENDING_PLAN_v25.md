# Pending Plan v25

- task: structural static-audit confirming v22 binding-layout-split patch is fully + correctly present in source tree, AND recording the next mechanically-actionable step under the terminal-block constraint
- source: no bundle — direct edit (file-only structural audit)
- approach: walk every site touched by v22 across the 4 source files (FGIPass.h, FGIPass.cpp, FRayTracingPipeline.h, FRayTracingPipeline.cpp) and confirm the patch shape matches the v21 plan intent. Also confirm v3, v5, v7, v8, v11, v12, v13, v14, v15, v17, v18, v19 instrumentation is present at the expected line numbers. This is the canonical "every patch still in source after many cycles" check. 0 source modifications; pure evidence-gathering.
- diff_estimate: +0 / -0 lines (read-only audit)
- skip_plan_review: no — the audit results inform the next decision-matrix routing; reviewer should sanity-check the audit conclusions
- test_strategy: cron verifies via static inspection (search_files + read_file at documented line ranges); Part B is parent-driven (run build + run_rgi_diagnostic.sh)
- risks:
  1. Audit misses a partial edit or untracked deletion — mitigation: walk the v22 commit's expected changes 1:1; if any are absent, the patch is incomplete (unlikely; v22 PENDING_IMPL_REVIEW was KEEP'd)
  2. The "next mechanically actionable step" turns out to require terminal access the cron does not have — mitigation: under the terminal-block constraint, the audit IS the next actionable step (state-machine gate); otherwise escalate via PIPELINE_HEALTH heartbeat
  3. Cross-file structural assumption about FRayTracingPipeline's State.addBindingSet pattern is wrong — mitigation: the audit verifies the 7-arg overload pattern (FRayTracingPipeline.cpp:316-322) and the new 6-arg overload (FRayTracingPipeline.cpp:344-372) have identical binding-set-add logic

## Why this is the right next cycle

The cron's "continue cycles ... until acceptance criteria are actually met" instruction authorizes mechanically-actionable file-only fixes. After v24's `dump_pixelstats.py` companion script, every diagnostic surface available without terminal has been built:

- v3: FGIPass + RenderGBuffer spdlog diagnostic markers (5 sites)
- v5: removed HLVM-bypass `close+execute+waitForIdle+open` block (v1-introduced)
- v7/v8: documentation drift cleanup (stale "HLVM-bypass" comments removed)
- v11/v12: cerr writes (default-ON after v12 macro removal)
- v13/v17/v18/v19: GIPathTracing.hlsl debug-mode probes (modes 1-15)
- v14: documentation drift cleanup (stale "line 675" → "line 691")
- v15: Private master GIPathTracing.hlsl sync with data-dir copy
- v20: run_rgi_diagnostic.sh one-shot bash runner
- v21: staged 9-branch decision matrix for post-parent-evidence routing
- v22: SRV/UAV binding-layout-split patch (the nvrhi-deferred-barrier-ordering hypothesis #1 fix)
- v23: dump-rotation off-by-one fix in run_rgi_diagnostic.sh
- v24: dump_pixelstats.py fast first-look diagnostic

Without terminal access, no further patch can be APPLIED AND VERIFIED. The next-best mechanically-actionable step is **structural verification that every patch survived prior cycles' evidence collection and remains in source**. A patch that was applied but later partially reverted or never reached the binary is the most common way an autonomous repair loop loses ground without noticing.

The cron can do this audit via `search_files` + `read_file` only. Each v3/v5/v7/v8/v11/v12/v13/v14/v15/v17/v18/v19/v22 site gets verified against the line numbers the prior commits claimed. Mismatches get flagged; matches get recorded.

## File-level changes

```
+ docs/PENDING_PLAN_v25.md        (this file)
+ docs/PENDING_PLAN_REVIEW_v25.md (post-review)
+ docs/PENDING_COMMIT_v25.md      (audit results recorded)
+ docs/PENDING_IMPL_REVIEW_v25.md (post-review)
+ docs/PENDING_TESTS_v25.md       (test surface)
+ docs/PENDING_TEST_AUDIT_v25.md  (final verdict)
M docs/PIPELINE_HEALTH_2026-07-27.md  (tick section appended)
```

0 source-code lines changed. Pure structural verification.

## Audit checklist (planned)

1. **v22 patch sites**: confirm `UAVBindingLayout` member present in FGIPass.h:106; `UAVBindingLayout = nullptr;` in FGIPass.cpp:183; `Device->createBindingLayout(UAVLayoutDesc)` in FGIPass.cpp:311; `Device->createBindingSet(UAVBuilder.Build(), UAVBindingLayout)` in FGIPass.cpp:595; new `DispatchRays(..., SRVBindingSet, UAVBindingSet)` overload in FRayTracingPipeline.h (search for "SRVBindingSet, UAVBindingSet" pattern); new `State.addBindingSet(SRVBindingSet.Get())` + `State.addBindingSet(UAVBindingSet.Get())` in FRayTracingPipeline.cpp:357-361.
2. **v3 diagnostic sites**: confirm LogGI at FGIPass.cpp:473 (EARLY-RETURN), FGIPass.cpp:555/568 (binding-set create + OK log), LogTest at TestReSTIR_GI_Temporal.cpp:445 (Pre-GIPass).
3. **v5 HLVM-bypass removal**: confirm no `close+execute+waitForIdle+open` block in TestReSTIR_GI_Temporal.cpp::RenderGBuffer; v5 NOTE comment present near line 1521.
4. **v7/v8 documentation drift**: confirm bug-088 paragraph at line 650-672 references v5 NOTE; v4a comment at line 1685-1693 reflects post-v5 state.
5. **v11/v12 cerr writes**: confirm `std::cerr << "[RGI] Render() entry:` at TestReSTIR_GI_Temporal.cpp:384 and `[RGI] FGIPass::DispatchRays() entry:` at FGIPass.cpp:462; no `#ifdef HLVM_FORCE_CERR_LOGGING` remaining; `<iostream>` includes present.
6. **v13/v17/v18/v19 sentinel probes**: confirm case 6u/7u/8u/9u/10u/11u/12u/15u/default-case in BOTH Private/Renderer/Shader/GI/GIPathTracing.hlsl AND TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl (per v15 sync).
7. **v14 line references**: confirm "line 691" at TestReSTIR_GI_Temporal.cpp:408, 662, 1537; no stale "line 675" remaining.
8. **v23 dump-rotation fix**: confirm `run_rgi_diagnostic.sh` uses archive-after-run pattern (not archive-before-run); v23 attribution in header.
9. **v24 dump_pixelstats.py**: confirm 166 lines, 6212 bytes, no broken patterns.
10. **PIPELINE_HEALTH append-only convention**: confirm last tick section is intact and properly delimited.

## What this audit does NOT do

- Does NOT modify any source code (read-only)
- Does NOT create Kanban cards (per cron instruction)
- Does NOT commit, push, or rewrite history (per cron instruction)
- Does NOT replace parent-driven verification (build + run + log + validator + vision)
- Does NOT advance the renderer toward acceptance criteria without terminal access

## What comes after v25

The audit either confirms everything is in source (clean KEEP) or flags a regression. If clean, the next cycle's recommendation is:
- Parent runs `bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh` (carries v3/v5/v7/v8/v11/v12/v13/v15/v17/v18/v19/v22 patches + v23 dump-rotation fix)
- Parent inspects `rgi_evidence.txt`
- Parent pastes back the evidence shape

If a regression is flagged, v26 stages the surgical restore to the prior commit's known-good state.

If the host's terminal block persists indefinitely, the pipeline stays at this heartbeat. Per the cron's "Never silently exit" hard rule, every tick must write SOMETHING to PIPELINE_HEALTH.