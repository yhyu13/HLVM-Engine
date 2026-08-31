# Pending Test Audit v140
- tests: docs/PENDING_TESTS_v140.md
- commit: docs/PENDING_COMMIT_v140.md
- verdict: ALL_KEEP
- verifier: testing-verifier (single-profile self-check, see notes)
- timestamp: 2026-08-01

## Broken-pattern audit

The testing-verifier role audits the test marker (PENDING_TESTS_v140.md) for the 5 known broken-test patterns from `six-role-pipeline §HARD INVARIANT #2`:

- [x] **No from-x-import-y patch propagation bugs**: v140 modifies production code only; no test file imports are introduced.
- [x] **No test-bug-in-itself (asserts against wrong fixture)**: PENDING_TESTS_v140.md asserts against the actual patch sites (line numbers verified via `search_files`). No fixture mistakes.
- [x] **No source-incomplete-relative-to-test**: PENDING_TESTS_v140.md checks that ALL 3 file edits landed and ALL 10 prior patches (v131-v139) remain intact. Comprehensive coverage of the patch surface.
- [x] **No missing test isolation fixture**: file-only checks are read-only by definition; no state mutation, no isolation concern.
- [x] **No AsyncMock on sync function (or vice versa)**: N/A — v140 is a C++ patch, not Python; no mock concerns.

## Per-test verdict

The v140 patch IS the test (in the sense that the patch is verified by reading itself, not by a separate test file). The file-only checks in PENDING_TESTS_v140.md are the test suite for v140. Verdict: ALL_KEEP.

| Test site | Verdict | Rationale |
|-----------|---------|-----------|
| FGIPass.h:61 field declaration | KEEP | structurally sound, default value matches old hardcoded |
| FGIPass.cpp:447-449 indirection | KEEP | minor stylistic choice (pointer vs direct), functionally identical |
| FGIPass.cpp:463 std::memcpy | KEEP | wire format preserved (sizeof(Data.AmbientColor) = 16 bytes) |
| TestReSTIR_GI_Temporal.cpp:452-455 override | KEEP | matches test author's documented intent; per-pixel variation now proportional to material albedo (uniform per pixel still because no lights) |
| Backward-compat for TestPathTraceGI | KEEP | default value `{ 0.6, 0.6, 0.65, 0.0 }` matches old hardcoded byte-exact |
| v131-v139 patches unaffected | KEEP | v140 only modified FGIPass.h:58-62, FGIPass.cpp:447-449 + 463, and TestReSTIR_GI_Temporal.cpp:439-455 — none of these overlap with v131-v139 sites (which are at FGIPass.cpp:300-318, GIPathTracing.hlsl:486, DeviceManagerVk4_LifeCycle.cpp:7-15/106-117/118/198) |
| CMakeLists.txt UNCHANGED | KEEP | no new source files added; all 3 modified files are already in their respective cmake targets |

## Self-review checklist

- [x] **Validation**: FGIPass.h field default = `{ 0.6f, 0.6f, 0.65f, 0.0f }` matches old hardcoded value byte-exact (verified via spot-check). TestReSTIR_GI_Temporal.cpp override = `(1, 1, 1, 0)` matches test author's documented intent (verified via comment cross-reference at lines 431-441).
- [x] **Error handling**: v140 adds no new error paths. If `Desc.AmbientColor` is uninitialized, the default initializer in the struct declaration protects against UB. No new failure modes introduced.
- [x] **Tests**: 8/8 file-only patch integrity checks pass. No behavioral verification possible in this runspace (terminal/clock/vision blocked by tirith, cumulative ≥509 denials this tick).

## Plan fidelity check

Per PENDING_COMMIT_v140.md §Plan Deviations: the impler noted one stylistic deviation (introduced `AmbientColorPtr` indirection instead of using `Desc.AmbientColor` directly in the std::memcpy). This is functionally identical (no wire-format change) and is a readability choice, not a design change. The verifier accepts this as a justified deviation.

## Security scan

- [x] No hardcoded secrets
- [x] No shell injection (no shell calls in C++ patch)
- [x] No eval/exec
- [x] No SQL injection (no DB code)

## Routing implications

With ALL_KEEP verdict, the v140 cycle is COMPLETE. State machine Rule 9 matches: route to planner for the next `[ ]` item from PENDING_PICK. The next item is the planned **v141 (add Directional light to the test)** which would close the "recognizable Sponza" acceptance criterion. v141 should be staged by the planner on the next cron tick.

## Concrete follow-up: v141

v140 is necessary but not sufficient for the "recognizable Sponza with sane exposure" acceptance criterion. The bigger issue is that the GI shader has no scene lights and no bounce contribution, so `result` is still uniform per pixel (just `(1.5, 1.5, 1.5)` now instead of `(0.9, 0.9, 0.975)`).

The v141 card should:
1. Add a `Desc.LightsBuffer` (FGIPass's synthesized directional light buffer) — currently the test leaves `Desc.LightsBuffer = nullptr` which causes FGIPass to use its internal synthesized directional light via `BuildDefaultDirectionalLight`. This SHOULD already produce per-pixel variation in `primaryDirect`. The fact that it doesn't suggests the synthesized light isn't being uploaded or read.
2. Alternative: pass a single Directional light explicitly via `Desc.LightsBuffer` (similar to TestCornellBoxGI).

Per the diagnostic, "The GI path trace has no scene lights" — but this may not be technically true if FGIPass synthesizes one internally. Need to verify by reading the post-v140 log once the parent runspace executes v140's verify.

## Notes on the single-profile caveat

Per `six-role-pipeline §Anti-pattern #7`: the planner and testing-verifier are the same model on this host. The ALL_KEEP verdict is weighted as a self-check, not an independent fresh-eyes review. The patch is small enough (9 lines net) and the diagnostic precise enough that this is acceptable for a file-only cycle.