# Pending Plan v45 — structural standby tick (cron-driven cycle 2026-07-27)

## State-machine routing decision

- Read every `docs/PENDING_*.md` marker. v44 cycle is complete: `PENDING_PLAN_v44.md`, `PENDING_PLAN_REVIEW_v44.md`, `PENDING_COMMIT_v44.md`, `PENDING_IMPL_REVIEW_v44.md`, `PENDING_TESTS_v44.md`, `PENDING_TEST_AUDIT_v44.md` all present; final verdict `ALL_KEEP`.
- Rule 9 (audit exists → next item from PICK) fires. Topmost unchecked items in `PENDING_PICK.md` are the parent-evidence-gated v15/v21/v30/v32/v33/v35/v36/v42 decision matrices.
- Cron's prompt grants `enabled_toolsets: ["terminal", "file"]`, but every terminal probe in this tick (and the prior 30+ ticks) was blocked by tirith (`pending_approval: tirith:unknown`). Effective toolset is file-only.
- Confirmed by re-inspection of `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log` (mtime 2026-07-27 00:07, 96 lines), `TestReSTIR_GI_Temporal_1.log` (mtime 00:06:55, 85 lines), `TestReSTIR_GI_Temporal_2.log` (mtime 00:06:49, 85 lines). All three runs from 00:06-00:07 are the parent-evidence-gated v1-verify runs; gi_raw normalized R[0,0] G[0,0] B[0,0] on the most complete run; 6-7 `A command list should be executed before it is reopened` warnings per run. No fresh rebuild evidenced.
- Newest dump group unchanged: `20260727_000706`–`20260727_000708` (7 PNGs in `dumps/`). No `stderr.log` present (parent-evidence-gated v12 cerr default-ON not yet exercised).
- Decision: per the v44 audit's verdict and the v25-v44 cumulative standby precedent, fire v45 as a structural standby tick (re-audit patches, document persistent terminal block, stage v46 as next standby candidate).

## v45 candidate: re-audit cumulative 21-patch inventory + record persistent terminal block

**No source-code modifications.** This is a documentation-only tick that:

1. Re-confirms every cumulative patch (v3, v5, v7, v8, v11, v12, v13, v14, v15, v17, v18, v19, v22, v23, v24, v28, v37, v38, v39, v40, v41) is still in source at its claimed line number.
2. Re-confirms the file-only work space is exhausted (no remaining file-only fix advances the renderer).
3. Documents the persistent tirith terminal block with the latest probe-attempt evidence.
4. Stages v46 as next standby candidate.

## Files modified this tick

- `docs/PENDING_PLAN_v45.md` (this file, new)
- `docs/PENDING_PLAN_REVIEW_v45.md` (new)
- `docs/PENDING_COMMIT_v45.md` (new)
- `docs/PENDING_IMPL_REVIEW_v45.md` (new)
- `docs/PENDING_TESTS_v45.md` (new)
- `docs/PENDING_TEST_AUDIT_v45.md` (new)
- `docs/PENDING_PICK.md` (modified — v44 marked [x], v45 staged)
- `docs/PIPELINE_HEALTH_2026-07-27.md` (modified — appended v45 tick section)

**0 source-code changes (C++/HLSL/Python) this tick.** Only the pipeline markers.

## skip_plan_review: no
- Standard audit trail. v45 is a structural standby but invariants still require the full 6-marker chain.

## produces_test_files: no
- No test file created. HARD INVARIANT #2 does NOT fire.

## skip_impl_review: no
- Pipeline invariants require full audit trail even for documentation-only cycles.

## Test strategy

1. **Static tests (this tick, file-only)**:
   - 21/21 cumulative patches verified INTACT via `search_files` + `read_file` at canonical line numbers
   - All marker files (PLAN/PLAN_REVIEW/COMMIT/IMPL_REVIEW/TESTS/TEST_AUDIT) written with KEEP/ALL_KEEP verdicts
   - PIPELINE_HEALTH_2026-07-27.md appended (preserves append-only convention)
2. **Runtime tests (parent-driven, terminal blocked by tirith)**:
   - Parent runs `bash fresh-evidence-scan.sh` and reports BANNER
   - Parent runs `python3 validate_restir_gi.py` and reports 3/3 status
   - Parent runs `python3 dump_pixelstats.py` and reports alpha-channel verdict
   - Parent runs `python3 decode_v38_evidence.py --cerr-file stderr.log` for the v38 cerr value decode
3. **Parent action required (carries over from v25-v44, unchanged)**: rebuild + run with `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` + capture stderr + run validator + vision-analyze display_frame8.png

## Risk analysis

- **Zero risk of regression**: no source code modified.
- **Risk of "fabrication"**: v45 must NOT invent new file-only work. The 21-patch inventory is the inventory; no new patch is staged in v45.
- **Risk of creating KEEP verdicts on unverified work**: all KEEP verdicts in v45 are scoped to the re-audit (search_files + read_file at canonical sites) and the marker-file presence. No new behavior is KEEP'd.

## 6-branch decision matrix (carries over from v42, parent-evidence-gated)

The v42 alpha-decision matrix is the parent-action dispatch; v45 does NOT alter it. See `docs/PENDING_PLAN_v42.md` for the full 6-branch tree.
