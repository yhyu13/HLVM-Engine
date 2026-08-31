# Pending Commit v35 — structural standby tick

## Files produced
- `docs/PENDING_PLAN_v35.md` (new)
- `docs/PENDING_PLAN_REVIEW_v35.md` (new)
- `docs/PENDING_COMMIT_v35.md` (new — this file)
- `docs/PENDING_IMPL_REVIEW_v35.md` (new)
- `docs/PENDING_TESTS_v35.md` (new)
- `docs/PENDING_TEST_AUDIT_v35.md` (new)
- `docs/PENDING_PICK.md` (modified — v35 marked [x], v36 staged as next standby candidate)
- `docs/PIPELINE_HEALTH_2026-07-27.md` (modified — appended v35 tick section)

## Source-code diff
- **0 source-code lines modified.** Pure documentation tick.
- No tests created or modified.
- No source-code patches reapplied (cumulative 18-patch inventory verified INTACT in source at start of tick).

## Verification
Source-code integrity: cumulative 18-patch inventory verified INTACT in source via `search_files` at start of this tick. Verified sites:
- **v3 spdlog markers** at FGIPass.cpp:511 (DispatchRays ENTER) + TestReSTIR_GI_Temporal.cpp Pre/Post-GIPass.
- **v5 HLVM-bypass removal** at TestReSTIR_GI_Temporal.cpp (the close+execute+waitForIdle+open block is removed).
- **v7/v8/v14 doc drift cleanup** at TestReSTIR_GI_Temporal.cpp:691 cross-references (3 sites).
- **v11/v12 cerr writes** default-ON at TestReSTIR_GI_Temporal.cpp:384 + FGIPass.cpp:487.
- **v13/v17/v18/v19 HLSL sentinels** at GIPathTracing.hlsl (Private + Data): cases 6u (line 593), 7u (line 604), 8u (line 614), 9u, 10u, 11u, 12u (line 663), 15u, default trace.
- **v15 Private master sync** — case 6u verified in Private copy.
- **v22 binding-layout-split** at FGIPass.h:106 (UAVBindingLayout member), FGIPass.cpp:183/281/282/296/311/312/596, FRayTracingPipeline.cpp:345/357/361/375/381 (2 overloads + UAVBindingSet param).
- **v23 dump-rotation archive-after-run** at run_rgi_diagnostic.sh.
- **v24 dump_pixelstats.py** companion script present.
- **v28 alpha-channel sentinel** at GIPathTracing.hlsl (Private + Data):694.
- **v32 fresh-evidence-scan.sh** orchestration helper present.
- **bug-088 executeCommandList** fix at TestReSTIR_GI_Temporal.cpp:691.

## Plan Deviations
- None. v35 implementation matches the v35 plan exactly.

## Notes for reviewer
- Tick is purely structural — no source code touched.
- Renderer state UNCHANGED: pipeline remains gated on parent rebuild + `fresh-evidence-scan.sh` invocation + paste-back.
- v33/v32/v30/v21 are the parent-evidence-gated active items in PENDING_PICK; v35 is the next standby slot per Rule 9.