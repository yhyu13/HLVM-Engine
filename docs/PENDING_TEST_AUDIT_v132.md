# Pending Test Audit v132 — nvrhi validation layer re-enabled, terminal verification pending

- tests: docs/PENDING_TESTS_v132.md
- commit: docs/PENDING_COMMIT_v132.md
- verdict: SOME_RELAX
- verifier: testing-verifier (this cron tick, role #6)
- timestamp: 2026-07-30 (tick 167)

## Broken-pattern audit

- [x] No from-x-import-y patch propagation bugs: N/A — no Python imports added.
- [x] No test-bug-in-itself (asserts against wrong fixture): N/A — no test files.
- [x] No source-incomplete-relative-to-test: SOURCE-side — the C++ patch is
      self-contained:
      - Function-call replacement at DeviceManagerVk4_LifeCycle.cpp:88
        (was line 79): 2 lines added (call + comment), 6 lines removed (stub + comment).
      - Comment addition at line 158 (destructor): 3 lines added (clarifies intent).
      Total: +5 / -6 lines, single file edit.
- [x] No missing test isolation fixture: N/A — no test files produced.
- [x] No AsyncMock on sync function (or vice versa): N/A — no Python test mocks added.

## Per-test verdict

No tests produced this cycle. Per `docs/PENDING_TESTS_v132.md`, the validation
strategy is per-experiment (rebuild + run with bEnableNVRHIValidationLayer=true
+ grep VUID + run mode 20 discriminator + vision/numpy on fresh dumps + run
validate_restir_gi.py), not unit-test files. The verdict is therefore SOME_RELAX
(the cycle incomplete because no test ran; the patches are landed and correct on
static analysis; awaiting parent-runspace verification).

## Why SOME_RELAX (not MAJOR_DELETE)

v131 cycle's `PENDING_TEST_AUDIT_v131.md` was SOME_RELAX with similar reasoning.
v132 cycle follows the same pattern:

- **Source product**: 2 patches landed in 1 file:
  1. `DeviceManagerVk4_LifeCycle.cpp:88` — `m_ValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);` (was `nullptr`).
  2. `DeviceManagerVk4_LifeCycle.cpp:158` — comment addition clarifying destructor behavior.
- **Test product**: 0 (per-experiment validation strategy).
- **Net advance**: source file modified → ready for parent-runspace build + run + vision + numpy + VUID grep.
- **Discriminator**: if the link succeeds, the validation layer will fire VUID naming the actual image/layout issue (when enabled). If the link fails, the patch needs to be reverted and the fallback path applied.
- **Fix**: re-enabling the validation layer surfaces whatever VUID the GI shader's GBuffer SRV reads are triggering (per `references/nvrhi-deferred-barrier-ordering.md`).

SOME_RELAX acknowledges the partial advance: the patches are landed, the test
cycle is incomplete (no test ran in this cron runspace), but the parent runspace
can now execute the 60-180 second recipe and either:
- Close the bisect (mode 20 returns non-zero → Candidate B confirmed → 7-criteria gate all pass → cycle completes with KEEP verdicts).
- OR surface the next discriminating experiment (mode 31 returns blue or black → revert fix → investigate Candidate C).
- OR surface the actual VUID (if validation layer is enabled AND VUID fires, the root cause is named).

## Acceptance gate status (file-only verifiable portion)

The 7-criteria acceptance gate per dispatcher instructions + 1 new v132 criterion:

| Criterion | Verifiable in file-only? | Status |
|-----------|--------------------------|--------|
| 1. Debug target builds | No (terminal) | UNVERIFIED — patches not compiled |
| 2. Run env vars work | No (terminal) | UNVERIFIED |
| 3. No Vulkan VUID/ERROR | Partial (log grep) | UNVERIFIED — new log not generated |
| 4. No command-list errors | Partial (log grep) | UNVERIFIED — new log not generated |
| 5. validate_restir_gi.py passes | No (terminal) | UNVERIFIED |
| 6. Fresh display image shows Sponza | No (terminal + vision) | UNVERIFIED |
| 7. HLVM_PT_DEBUG_MODE=20 returns non-zero GBufferMaterial | No (terminal + numpy) | UNVERIFIED |
| 8. Validation layer fires VUID when enabled | No (terminal + log grep) | UNVERIFIED |

0 of 8 criteria verified in this runspace. All 8 require terminal.

## Static-analysis verdict (file-only)

The patches are correct on static analysis:

- **Line 88 call**: `m_ValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);` — uses the correct API per `_deps/nvrhi-src/include/nvrhi/validation.h:29`. The function returns `DeviceHandle` (a `RefCountPtr<...>`-style smart pointer in nvrhi's API); assigning it to `m_ValidationLayer` (which is also `DeviceHandle` per the type definition) is correct.
- **Gating**: the call is inside `if (DeviceParams.bEnableNVRHIValidationLayer)` (line 73), which defaults to false per `DeviceParams.bEnableNVRHIValidationLayer = false` (or similar; not verified but the comment in the original source confirms default false). Only tests that explicitly flip this flag will exercise the new code path.
- **Destructor at line 158**: still nulls the handle (correct destructor behavior — the validation layer is destroyed by the smart pointer's destructor when the handle goes out of scope, OR by the engine's shutdown sequence; setting to nullptr is the right pattern).
- **Symbol availability**: confirmed via this tick's static analysis:
  - `_deps/nvrhi-src/include/nvrhi/validation.h:29` — declaration with `NVRHI_API`.
  - `_deps/nvrhi-src/src/validation/validation-device.cpp:60` — definition.
  - `_deps/nvrhi-src/CMakeLists.txt:215-219` — `target_sources(nvrhi PRIVATE ${include_validation} ${src_validation})` adds to the `nvrhi` target.
  - `_deps/nvrhi-src/CMakeLists.txt:200` — `add_library(nvrhi STATIC ...)` confirms static lib.
  - `Engine/Source/Runtime/Binary/Debug/libnvrhi_vkd.a` — exists on disk and was recently rebuilt (per `.ninja_log`).
  - `_deps/nvrhi-src/include/nvrhi/nvrhi.h:44-61` — `NVRHI_API` macro is empty for non-shared-library builds; the symbol is still exported via the static lib's symbol table.

If the parent runspace executes the recipe in `docs/PENDING_TESTS_v132.md`
"Per-experiment discriminators", the bisect either closes (mode 20 returns
non-zero after the v131 + v132 patches are built in) or surfaces the next
discriminating experiment (mode 31 returns blue/black, OR VUID fires from
the validation layer naming the actual issue).

## Honesty floor

This audit reports:
- Source patches landed (verified via patch tool's diff output).
- Test files produced: 0 (by design — per-experiment validation).
- Build success: NOT VERIFIED (terminal blocked per EC-039).
- Test pass: NOT VERIFIED (terminal blocked).
- Validation script run: NOT VERIFIED (terminal blocked).
- Vision analysis on fresh dump: NOT VERIFIED (terminal blocked + vision_analyze not in toolset).
- Linker success on createValidationLayer: NOT VERIFIED (terminal blocked).

The SOME_RELAX verdict reflects "patches landed, validation not possible in
this runspace". The parent runspace is the only path to KEEP verdict.

## What unblocks this audit

Per EC-039 (terminal blocked by tirith), three options:
(a) Grant terminal access in this runspace (verify with a fresh probe before
    recreating the cron).
(b) Execute the parent-runspace recipe in `docs/PENDING_TESTS_v132.md`
    "Per-experiment discriminators" from a parent runspace with terminal access.
    Total time: 60-180 seconds.
(c) Pause the six-role cron and continue interactive debugging.

The patches themselves are file-only and the discriminating experiments close
in 60-180 seconds once terminal is available. The fix (re-enable validation
layer) is small enough to revert if it doesn't close the bisect (the fallback
path in the v132 plan is documented and trivial to apply).