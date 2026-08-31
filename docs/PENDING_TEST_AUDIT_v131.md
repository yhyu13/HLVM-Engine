# Pending Test Audit v131 — patches landed, terminal verification pending

- tests: docs/PENDING_TESTS_v131.md
- commit: docs/PENDING_COMMIT_v131.md
- verdict: SOME_RELAX
- verifier: testing-verifier (this cron tick, role #6)
- timestamp: 2026-07-30 (tick 151)

## Broken-pattern audit

- [x] No from-x-import-y patch propagation bugs: N/A — no Python
      imports added; the patches are HLSL/C++ only.
- [x] No test-bug-in-itself (asserts against wrong fixture): N/A — no
      test files produced.
- [x] No source-incomplete-relative-to-test: SOURCE-side — the HLSL
      and C++ patches landed in this cycle are self-contained:
      - Step A bypass list addition: +4 lines (comment + 31u entry).
      - Step A case 31u discriminator: +18 lines (new case block).
      - Step B fix: +13 lines (comment + commitBarriers() call).
      All three changes are minimal and structurally isolated.
- [x] No missing test isolation fixture: N/A — no test files
      produced.
- [x] No AsyncMock on sync function (or vice versa): N/A — no
      Python test mocks added.

## Per-test verdict

No tests produced this cycle. Per `docs/PENDING_TESTS_v131.md`, the
validation strategy is per-experiment (vision + numpy on fresh dumps
after running `HLVM_PT_DEBUG_MODE=20` and `31`), not unit-test files.
The verdict is therefore SOME_RELAX (the cycle incomplete because
no test ran; the patches are landed and correct on static analysis;
awaiting parent-runspace verification).

## Why SOME_RELAX (not MAJOR_DELETE)

v129 cycle's `PENDING_TEST_AUDIT_v129.md` was MAJOR_DELETE because
the cycle produced no test product AND no source product (terminal
blocked, impler honest exit). v131 cycle is different:

- **Source product**: 4 patches landed across 3 files:
  1. `GIPathTracing.hlsl` Private: bypass list includes 31u (4 lines)
  2. `GIPathTracing.hlsl` Private: case 31u discriminator (18 lines)
  3. `GIPathTracing.hlsl` Data: mirror copy of patches 1+2
  4. `FGIPass.cpp`: commitBarriers() fix before DispatchRays (13 lines)
- **Test product**: 0 (per-experiment validation strategy).
- **Net advance**: source files modified → ready for parent-runspace
  build + run + vision + numpy.
- **Discriminator**: case 31u is the discriminator for Candidate A.
- **Fix**: commitBarriers() addresses Candidate B if the static-analysis
  evidence is correct.

SOME_RELAX acknowledges the partial advance: the patches are landed,
the test cycle is incomplete (no test ran in this cron runspace),
but the parent runspace can now execute the 60-180 second recipe
described in `docs/PENDING_TESTS_v131.md` "Per-experiment discriminators"
and either:

- Close the bisect (mode 20 returns non-zero → Candidate B confirmed → 7-criteria gate all pass → cycle completes with KEEP verdicts),
- OR surface the next discriminating experiment (mode 31 returns blue or black → revert fix → investigate Candidate C).

## Acceptance gate status (file-only verifiable portion)

The seven-criteria acceptance gate per dispatcher instructions:

| Criterion | Verifiable in file-only? | Status |
|-----------|--------------------------|--------|
| 1. Debug target builds | No (terminal) | UNVERIFIED — patches not compiled |
| 2. Run env vars work | No (terminal) | UNVERIFIED |
| 3. No Vulkan VUID/ERROR | Partial (log grep) | UNVERIFIED — new log not generated |
| 4. No command-list errors | Partial (log grep) | UNVERIFIED — new log not generated |
| 5. validate_restir_gi.py passes | No (terminal) | UNVERIFIED |
| 6. Fresh display image shows Sponza | No (terminal + vision) | UNVERIFIED |
| 7. HLVM_PT_DEBUG_MODE=20 returns non-zero GBufferMaterial | No (terminal + numpy) | UNVERIFIED |

0 of 7 criteria verified in this runspace. All 7 require terminal.

## Static-analysis verdict (file-only)

The patches are correct on static analysis:

- **Bypass list**: 31u added to the existing 20u/21u/22u/30u list at
  GIPathTracing.hlsl:472-480 (both copies). Pattern matches the
  tick-135 fix (which added 30u).
- **Case 31u discriminator**: new case block at GIPathTracing.hlsl:705-720
  (both copies). Uses the same `case N: { ... break; }` block structure
  as the existing cases (20/21/22/30). The arithmetic transformation
  (`r * 0.5 + 0.1`) is non-trivial and on the debugColor data path,
  making it observable to slangc's reachability analysis.
- **commitBarriers() fix**: added at FGIPass.cpp:656-668, immediately
  before RTPipeline.DispatchRays call. The fix is idempotent
  (commitBarriers on no pending barriers is a no-op). The fix doesn't
  change any other code path.

If the parent runspace executes the recipe in `docs/PENDING_TESTS_v131.md`
"Per-experiment discriminators", the bisect either closes (Candidate B
was the root cause) or surfaces the next discriminating experiment
(Candidate A or C discriminator).

## Honesty floor

This audit reports:
- Source patches landed (verified via read_file offsets documented in
  PENDING_COMMIT_v131.md).
- Test files produced: 0 (by design — per-experiment validation).
- Build success: NOT VERIFIED (terminal blocked per EC-039).
- Test pass: NOT VERIFIED (terminal blocked).
- Validation script run: NOT VERIFIED (terminal blocked).
- Vision analysis on fresh dump: NOT VERIFIED (terminal blocked +
  vision_analyze not in toolset).

The SOME_RELAX verdict reflects "patches landed, validation not
possible in this runspace". The parent runspace is the only path
to KEEP verdict.

## What unblocks this audit

Per EC-039 (terminal blocked by tirith), three options:

(a) Grant terminal access in this runspace (verify with a fresh
    probe before recreating the cron).
(b) Execute the parent-runspace recipe in `docs/PENDING_TESTS_v131.md`
    "Per-experiment discriminators" from a parent runspace with
    terminal access. Total time: 60-180 seconds.
(c) Pause the six-role cron and continue interactive debugging.

The patches themselves are file-only and the discriminating experiments
close in 60-180 seconds once terminal is available. The fix
(commitBarriers) is small enough to revert if it doesn't close the
bisect.