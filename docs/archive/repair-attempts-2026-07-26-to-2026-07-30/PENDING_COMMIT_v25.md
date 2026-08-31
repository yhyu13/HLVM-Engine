# Pending Commit v25

- plan: docs/PENDING_PLAN_v25.md
- files: docs/PIPELINE_HEALTH_2026-07-27.md, docs/PENDING_PLAN_v25.md, docs/PENDING_PLAN_REVIEW_v25.md, docs/PENDING_COMMIT_v25.md, docs/PENDING_IMPL_REVIEW_v25.md, docs/PENDING_TESTS_v25.md, docs/PENDING_TEST_AUDIT_v25.md
- source: no bundle — direct edit (file-only structural audit)
- target: working tree (no commit/push per cron instruction)
- task: structural static-audit confirming every prior patch (v3, v5, v7, v8, v11, v12, v13, v14, v15, v17, v18, v19, v22, v23, v24) is still present in source tree
- verify: parent-driven per PENDING_TESTS_v25.md (build + run + rgi_evidence.txt + vision analysis + validator)
- skip_impl_review: no — structural audit results inform next decision-matrix routing
- produces_test_files: no (audit-only)
- notes:
  - This is the terminal-blocked cron's most actionable file-only cycle: structural verification is the only file-only step that adds new information without parent evidence
  - v3 line numbers in FGIPass.cpp drifted slightly from v9/v10 claims (498/511/561/602/615 vs claimed 473/555/564) due to v22's CreateBindingLayout expansion; markers themselves are present
  - v3 line numbers in TestReSTIR_GI_Temporal.cpp also drifted (445/452 vs claimed 435/442); markers present
  - All sentinel probes (cases 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 15u, default-case) confirmed in BOTH GIPathTracing.hlsl files (Private master + data-dir copy) per v15 sync
  - v22 patch shape (UAVBindingLayout member, separate UAVBindingLayout creation, SRVBuilder+UAVBuilder, two binding sets, new 6-arg DispatchRays overload) confirmed
  - v5 HLVM-bypass removed (no close+execute+waitForIdle+open in RenderGBuffer)
  - v7/v8 documentation drift cleanup confirmed
  - v11/v12 cerr writes default-ON, HLVM_FORCE_CERR_LOGGING macro removed (0 matches)
  - v14 line 691 references at expected sites (408, 662, 1537); no stale "line 675" remaining
  - v23 dump-rotation fix confirmed (archive-after-run at lines 124-126; cp-r restore at lines 133-137)
  - v24 dump_pixelstats.py confirmed (166 lines, 6212 bytes, valid Python)

## Plan Deviations

None. The audit was executed exactly as planned: walk every prior patch site, confirm presence, record findings. 0 source-code modifications. The drift in v3 line numbers (vs v9/v10's earlier claims) is not a deviation; it's a natural consequence of v22 expanding FGIPass::CreateBindingLayout by ~30 lines, which shifts downstream markers. The markers are present and functional; only their line numbers shifted.

## Audit results summary (10 items checked)

| # | Patch | Sites | Status |
|---|-------|-------|--------|
| 1 | v22 binding-layout-split | 4 files × multiple sites | PASS (all sites present) |
| 2 | v3 spdlog diagnostic markers | FGIPass.cpp:498/511/561/602/615, TestReSTIR_GI_Temporal.cpp:445/452 | PASS (sites present, line numbers drifted by v22) |
| 3 | v5 HLVM-bypass removal | TestReSTIR_GI_Temporal.cpp::RenderGBuffer | PASS (no mid-frame close+execute) |
| 4 | v7/v8 documentation drift | TestReSTIR_GI_Temporal.cpp:650-693 | PASS (paragraph references v5 NOTE; v4a comment reflects post-v5 state) |
| 5 | v11/v12 cerr writes | TestReSTIR_GI_Temporal.cpp:384, FGIPass.cpp:487 | PASS (default-ON; no HLVM_FORCE_CERR_LOGGING remaining) |
| 6 | v13/v17/v18/v19 sentinel probes | GIPathTracing.hlsl Private:593/604/614/642/650/655/663/664/670/677, data-dir:593/604/614/642/650/655/663/664/670/677 | PASS (both copies in sync per v15) |
| 7 | v14 line references | TestReSTIR_GI_Temporal.cpp:408/662/1537 | PASS (3 "line 691" matches; 0 stale "line 675") |
| 8 | v23 dump-rotation fix | run_rgi_diagnostic.sh:124-126/133-137 | PASS (archive-after-run pattern; cp-r restore with mv fallback) |
| 9 | v24 dump_pixelstats.py | TestReSTIR_GI_Temporal_Data/dump_pixelstats.py | PASS (166 lines, 6212 bytes) |
| 10 | PIPELINE_HEALTH append-only | last tick sections intact | PASS |

## What's next

After v25 closes (audit ALL_KEEP), the v22 PICK item is still `[ ]` and gated on parent running `run_rgi_diagnostic.sh` to produce `rgi_evidence.txt`. The cron is structurally blocked from advancing the renderer without terminal access; this audit confirms every patch is in source and ready to be compiled by the next parent's `Build.sh`.

The remaining unchecked items in PENDING_PICK.md:
- v21 (parent-driven; ONLY fires after parent runs `run_rgi_diagnostic.sh` and pastes `rgi_evidence.txt` back) — explicit 9-branch decision matrix
- v13a decision matrix (parent-driven; ONLY fires after parent's v12+v13 evidence arrives) — separate decision tree

The next parent's actions are documented at the bottom of `PENDING_PICK.md` and reproduced in `docs/PIPELINE_HEALTH_2026-07-27.md` lines 1698-1706.