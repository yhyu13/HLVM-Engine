# Pending Tests v10
- tests: docs/PENDING_TESTS_v10.md
- commit: docs/PENDING_COMMIT_v10.md
- tester: tester (single-head autonomous cron — software-development-practices §"Full auto" anti-pattern #7 caveat applies)
- timestamp: 2026-07-27T08:58:00Z (estimated cron tick wall clock)

## Test surface analysis

This cycle is documentation-only. No source files modified. No behavioral changes. No new test surface to test.

## Existing test surface (unchanged)

- `validate_restir_gi.py` — Python validator. Continues to apply against post-rebuild dumps. The validator checks 4 structural properties (black-pixel ratio, color variance, temporal stability, cell variance). v10's documentation cycle does not affect the validator.
- v3's diagnostic logs in TestReSTIR_GI_Temporal.cpp:435/442 and FGIPass.cpp:460/467/473/552/555/564 — these are the test surface that the parent will exercise after rebuild. They will fire per frame (5 lines/frame × 8 frames = 40 log lines per run) IF the source/binary mismatch has been resolved by rebuild.
- `validate_restir_gi.py` exit codes (0 = pass, 1 = fail) are the canonical "this test works" gate.

## New tests added

None. Documentation-only cycle.

## What v10 does NOT require from a testing perspective

- No new test files
- No new test code
- No new validator improvements
- No new ShaderMake changes

## What v10 DOES record (test-relevant observations)

1. The source/binary mismatch is CONFIRMED by static line-number evidence.
2. After parent rebuild, the v3 diagnostic logs will fire per frame and reveal:
   - Whether the dispatch enters (Pre-GIPass + DispatchRays ENTER fire)
   - Whether the dispatch returns normally (Post-GIPass + DispatchRays EXIT fire)
   - Whether the binding set was created (per-frame binding set OK fires)
   - Whether the output texture received a write (gi_raw dump shows non-zero)
3. If after rebuild v3 logs STILL don't appear → that's a separate diagnostic surface bug requiring deeper investigation (v10b/v10c).

## Acknowledged: testing is parent-driven

The cron cannot run `validate_restir_gi.py` without terminal. The parent must rebuild + re-run + run validator. v10's tester role records this acknowledgment honestly.
