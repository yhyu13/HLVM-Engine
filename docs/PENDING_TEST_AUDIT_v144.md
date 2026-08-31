# Pending Test Audit v144
- tests: docs/PENDING_TESTS_v144.md
- commit: docs/PENDING_COMMIT_v144.md
- verdict: ALL_KEEP
- verifier: testing-verifier (single-profile self-check)
- timestamp: 2026-08-05

## Broken-pattern audit

- [x] No from-x-import-y patch propagation bug: the test imports only the pure selector module and reads build files via `Path`; it does not patch imports or mocks.
- [x] No test-bug-in-itself: the expected whole-archive string exactly matches both applied edits, and the rejected old strings exactly match the pre-v144 linkage forms.
- [x] No source incomplete relative to test: both the generator source and generated CMake are changed and both are asserted; the CMake-version evidence exists at the cited build path.
- [x] No missing test isolation fixture: tests are read-only, use synthetic paths or repository files, and do not alter environment/global state.
- [x] No AsyncMock on sync function (or vice versa): not applicable; no mocks or async code are used.

## Per-test verdict

- [x] `test_validate_restir_gi.py::test_returns_all_files_when_no_display_anchor_exists`: KEEP — retained v143 pure-selector regression.
- [x] `test_validate_restir_gi.py::test_selects_latest_run_when_each_run_spans_multiple_seconds`: KEEP — retains same-second channel ordering coverage.
- [x] `test_validate_restir_gi.py::test_excludes_partial_stale_files_before_latest_display`: KEEP — retains stale-prefix exclusion coverage.
- [x] `test_validate_restir_gi.py::test_current_dump_directory_matches_latest_display_timestamp`: KEEP — read-only current-dump contract.
- [x] `test_validate_restir_gi.py::test_device_validation_is_configured_before_creation`: KEEP — retains v143 lifecycle ordering contract.
- [x] `test_validate_restir_gi.py::test_nvrhi_validation_archive_is_forced_into_runtime_link`: KEEP — directly covers the v144 source/generated linkage change and rejects regression to the known failing form.

## Testability gap audit

The static test set covers the v144 source contract and retains all v143 file-only checks. It cannot establish linker success, Vulkan layer activation, absence of fresh VUID/ERROR/command-list messages, newest-group validator output, mode-20 non-zero pixels, or visual Sponza structure; those are correctly specified as runtime gates in `PENDING_COMMIT_v144.md` and remain unexecuted.

## Runtime status

ALL_KEEP applies to test quality, not behavioral acceptance. Terminal execution is denied by `tirith:unknown`, and no vision tool is available. No runtime result is fabricated.
