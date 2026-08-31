# Pending Test Audit v37 — alpha-channel sentinel awareness in validate_restir_gi.py

## Verdict
- **ALL_KEEP** — validator modification is purely additive, mechanically correct, exhaustive in its 5-alpha-pattern verdict ladder, and ready for parent terminal-driven verification.

## Broken-pattern audit
- [x] No from-x-import-y patch propagation bugs (N/A — single-file Python validator)
- [x] No test-bug-in-itself: the new check_alpha_sentinel reads the actual RGBA data and emits a single-line verdict; does not assert against an imagined contract
- [x] No source-incomplete-relative-to-test (validator IS the test surface)
- [x] No missing test isolation fixture (N/A — validator script reads from dumps/ directory)
- [x] No AsyncMock on sync function or vice versa (N/A — pure Python + numpy)

## Per-test verdict
- A1-A22: 22/22 PASS (static file-only verification)
- B1-B8: 8/8 UNVERIFIED (parent-driven, terminal blocked by tirith)
- C1-C6 (goal gate): UNVERIFIED — six criteria from prompt all require parent action

## Per-part verdict
- Part A (static): ALL_KEEP — 22/22 mechanical checks pass.
- Part B (runtime): UNVERIFIED — parent-driven, terminal required.
- Part C (goal gate): UNVERIFIED — six criteria from prompt remain unchanged.

## Specific audit findings
1. **Backwards compatibility verified**: pre-v28 dumps will trigger the `alpha=low` FAIL verdict (sentinel not in compiled shader). This is the CORRECT diagnostic outcome — not a regression. The pre-v28 binary's alpha channel contains legitimate `avgFirstHitDist` values which are typically 0.01-0.05, well within `alpha<=50` low-range.
2. **No source-code touch**: validator is the only modified file. Cumulative 22-patch inventory (v3 through v36) verified intact via static inspection at start of tick.
3. **HARD INVARIANT #2 satisfied**: this IS a test file modification. Full reviewer → tester → testing-verifier chain was invoked (markers PENDING_IMPL_REVIEW_v37 + PENDING_TESTS_v37 + PENDING_TEST_AUDIT_v37 all present with KEEP/ALL_KEEP verdicts).
4. **The 5-alpha-pattern verdict ladder correctly handles all evidence shapes**:
   - alpha=saturated → dispatch ran → bug is downstream
   - alpha=0 → dispatch didn't run → bug is upstream (binding layout, command list)
   - alpha=mixed → partial dispatch → bug is per-tile (likely barrier partial)
   - alpha=low → pre-v28 binary → parent must rebuild
   - no-dump → validator cannot find display_frame8.png → parent must run with HLVM_DUMP_RGI=1
5. **Cumulative impact on goal gate**: criterion (e) "Validator passes newest dump group" was UNVERIFIED before v37 and remains UNVERIFIED until parent rebuilds and runs. v37 ADDS the alpha-evidence signal that makes (e) much more diagnostic when the parent runs — i.e., v37 makes criterion (e) verifiable on the next parent terminal run, where before v37 it was unverifiable except as a binary PASS/FAIL on RGB.

## Single-head caveat
- Same model writes all 6 roles. Verdicts are self-checks. The patch is purely additive (existing 3 RGB checks unchanged; new 4th check is independent) so the verdict is reproducible.

## Goal gate
- FAILED/UNVERIFIED — six-criterion gate from prompt remains unchanged. No `PIPELINE_GOAL_DONE_<date>.md` written.

## Recommendation
- KEEP. v37 cycle complete. v38 staged as next decision-matrix target based on the alpha evidence shape that surfaces on parent's next terminal run.