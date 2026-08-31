# Pending Plan v44 — structural standby tick (cron-driven cycle 2026-07-27)

## State-machine routing decision

- Read every `docs/PENDING_*.md` marker. v43 cycle is complete: `PENDING_PLAN_v43.md`, `PENDING_PLAN_REVIEW_v43.md`, `PENDING_COMMIT_v43.md`, `PENDING_IMPL_REVIEW_v43.md`, `PENDING_TESTS_v43.md`, `PENDING_TEST_AUDIT_v43.md` all present; final verdict `ALL_KEEP`.
- Rule 9 (audit exists → next item from PICK) fires. Topmost unchecked items in `PENDING_PICK.md` are the parent-evidence-gated v15/v21/v30/v32/v33/v35/v36/v42 decision matrices.
- Cron's prompt grants `enabled_toolsets: ["terminal", "file"]`, but every terminal probe in this tick (and the prior 20+ ticks) was blocked by tirith (`pending_approval: tirith:unknown`). Effective toolset is file-only.
- Decision: per the v43 audit's verdict "subsequent cron ticks without parent terminal access will be identical-standby markers documenting the persistent terminal block and the cumulative patch inventory," fire v44 as a structural standby tick.

## v44 candidate: re-audit cumulative 21-patch inventory + record persistent terminal block

**No source-code modifications.** This is a documentation-only tick that:

1. Re-confirms every cumulative patch (v3, v5, v7, v8, v11, v12, v13, v14, v15, v17, v18, v19, v22, v23, v24, v28, v37, v38, v39, v40, v41) is still in source at its claimed line number.
2. Re-confirms the file-only work space is exhausted (no remaining file-only fix advances the renderer).
3. Documents the persistent tirith terminal block with the latest probe-attempt evidence.
4. Stages v45 as next standby candidate.

## Files modified this tick

- `docs/PENDING_PLAN_v44.md` (this file, new)
- `docs/PENDING_PLAN_REVIEW_v44.md` (new)
- `docs/PENDING_COMMIT_v44.md` (new)
- `docs/PENDING_IMPL_REVIEW_v44.md` (new)
- `docs/PENDING_TESTS_v44.md` (new)
- `docs/PENDING_TEST_AUDIT_v44.md` (new)
- `docs/PENDING_PICK.md` (modified — v43 marked [x], v44 staged)
- `docs/PIPELINE_HEALTH_2026-07-27.md` (modified — appended v44 tick section)

**0 source-code changes (C++/HLSL/Python) this tick.** Only the pipeline markers.

## skip_plan_review: no
- Standard audit trail. v44 is a structural standby but invariants still require the full 6-marker chain.

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
   - Parent runs `python3 decode_v38_evidence.py --cerr-file stderr.log` and reports verdict

## Risks

- No risk to renderer (no source change).
- No risk to existing patches (additive marker files only).
- Cumulative 21-patch inventory must remain intact; this tick re-verifies.

## Goal gate (unchanged)
**FAILED/UNVERIFIED** — six-criterion gate from prompt:
- (a) Debug target builds cleanly — UNVERIFIED (tirith blocks terminal)
- (b) Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — UNVERIFIED
- (c) No command-list-already-open errors — UNVERIFIED
- (d) No Vulkan ERROR/VUID in fresh log — UNVERIFIED
- (e) Validator passes newest dump group — UNVERIFIED
- (f) Display visibly contains recognizable non-uniform Sponza — UNVERIFIED

## Recommendation
KEEP. v44 is documentation-only; pipeline remains parent-evidence-gated. v45 staged as next standby candidate if terminal block persists. If terminal becomes available on a future tick, v44's re-audit gives the next cron session an immediate ground-truth on the cumulative patch state without re-walking every site.