# Pending Test Audit v143
- tests: docs/PENDING_TESTS_v143.md
- commit: docs/PENDING_COMMIT_v143.md
- verdict: ALL_KEEP
- verifier: testing-verifier (single-profile self-check)
- timestamp: 2026-08-03

## Broken-pattern audit

- [x] No from-x-import-y patch propagation bug: no mocks/import patching.
- [x] No test bug in itself: synthetic grouping expectations follow the actual display-first C++ dump order and explicitly cover same-second channels that sort before `display`.
- [x] No source incomplete relative to test: both flags, consolidated pre-device block, timestamp helper, and five cited test cases are present.
- [x] No missing isolation fixture: synthetic helper cases use in-memory strings; current-dump and C++ source-contract cases are read-only and perform no mutation.
- [x] No AsyncMock on sync function: not applicable.

## Per-test verdict

- [x] `test_validate_restir_gi.py` (5 module-direct/source-contract cases): KEEP — pure/read-only tests cover no-anchor passthrough, multiple runs spanning seconds, same-second ordering, partial stale-prefix exclusion, current-dump latest-display selection, and C++ pre-device option ordering without mutating files/global state.
- [x] C++ pre-creation source ordering: KEEP — exercised by the source-contract case; the consolidated parameters occur before the creation call, match the proven sibling pattern, and the old late block is gone.
- [x] GPU behavioral recipe: KEEP AS SPECIFICATION — commands and expected evidence correctly exercise every integration gate; execution status is tracked separately below.
- [x] Vision criterion: KEEP AS SPECIFICATION — it correctly requires inspection of the fresh default-mode display rather than treating mode 20 as the final image.

## Testability gap audit

The test definitions cover the v143 patch and all plan acceptance surfaces without any of the five broken-test patterns. **ALL_KEEP applies to test quality, not runtime success.** Target build, system validation-layer activation, fresh zero-VUID/error scan, newest-group 4/4 output, non-zero mode 20, and visual Sponza remain unexecuted because the scheduled worker's terminal and vision tools are blocked. The pipeline code/test cycle is complete, but the user-facing acceptance gate is not closed.

## Required re-audit evidence

1. Python regression tests pass.
2. Real build exits 0.
3. Fresh default run with `HLVM_RGI_ACCUM=8` exits 0.
4. Fresh log explicitly lists `VK_LAYER_KHRONOS_validation` and has zero `VUID|ERROR|command[- ]list` hits.
5. Validator reports 4/4 and selects only files at/after the newest display timestamp.
6. Fresh mode-20 gi_raw statistics are non-zero.
7. Fresh default display is visually recognizable Sponza with sane exposure.

No source/test deletion is requested. The pending items require execution, not code relaxation.
