# Pending Impl Review v40 — extend dump_pixelstats.py to read alpha channel

## Verdict: KEEP

## plan_fidelity_check
The implementation matches the v40 plan exactly:
- New functions added: `compute_alpha_stats(arr)`, `classify_alpha_sentinel(stats, saturated_min, low_max)`
- `emit_stats()` extended with a v40 alpha-inspection block that re-opens the PNG in RGBA mode and emits an `A:` stats line + `[v40-alpha]` verdict line
- 5-pattern verdict ladder (saturated / zero / mixed / low / unknown) matches v37's `check_alpha_sentinel()` ladder
- Banner header updated from `v24` to `v24 + v40`
- Docstring extended with 12-line v40 history paragraph
- Pure additive change: existing RGB stats block is byte-identical to v24

No deviations from the plan.

## TDD evidence
- [ ] Test file present: not applicable (no test file modified)
- [ ] Test commit precedes impl: not applicable
- [ ] Red-phase commit message: not applicable

This is a diagnostic helper extension, not test-driven production code. The "tests" are the runtime runs parent executes on next dump group, which cron cannot drive (terminal blocked).

## Security scan
- [x] No hardcoded secrets
- [x] No shell injection (pure Python parser, no subprocess)
- [x] No eval/exec
- [x] No SQL injection (no SQL)
- [x] No buffer overflows (Python; no manual memory)
- [x] Best-effort alpha inspection is wrapped in try/except so RGB reporting is preserved

## Self-review checklist
- [x] Validation: alpha stats computed correctly (mean/std/unique/sat255/sat0) using numpy
- [x] Error handling: alpha read failures are swallowed (best-effort); RGB block runs regardless
- [x] Tests: 22 static tests in PENDING_TESTS_v40.md; runtime tests parent-driven
- [x] Plan fidelity: matches plan exactly (no deviations)

## Mid-flight corrections
None. Patch applied cleanly on first attempt.

## Feedback for impler (FIX only)
(none — patch matches plan)

## Verdict: KEEP