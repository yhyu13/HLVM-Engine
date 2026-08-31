# Pending Test Audit v139 — Re-enable nvrhi validation layer hookup

- tests: docs/PENDING_TESTS_v139.md
- commit: docs/PENDING_COMMIT_v139.md
- verdict: ALL_KEEP
- verifier: testing-verifier (file-only single-profile mode, tick 264)
- timestamp: 2026-08-01

## Broken-pattern audit

- [x] No from-x-import-y patch propagation bugs (no test file produced, no propagation risk)
- [x] No test-bug-in-itself (no test file produced)
- [x] No source-incomplete-relative-to-test (source has v139 patch; no test written against it)
- [x] No missing test isolation fixture (no test written)
- [x] No AsyncMock on sync function (N/A — C++/GPU rendering, no mocks)
- [x] No GPU binding-pattern regression introduced (v137+v138 patches preserved per impl-review evidence)
- [x] No revert of v131+v135+v137+v138 diagnostic chain (file inspection confirms intact)

## Per-test verdict

This is a diagnostic-patch cycle (no behavioral test file produced). The "test" is the next successful rebuild + behavior-change verification. That requires terminal+vision which is structurally blocked in this cron runspace per EC-039. All file-only tests run by the tester (8 PASS in PENDING_TESTS_v139.md §"File-only verification") were re-confirmed this audit:

1. Include addition at line 15 of DeviceManagerVk4_LifeCycle.cpp — re-verified via read_file.
2. Comment block at lines 7-14 — re-verified.
3. Stub → call edit at line 118 — re-verified: `m_ValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);` is present inside the gated `if (DeviceParams.bEnableNVRHIValidationLayer)` block at lines 82-119.
4. v139 rationale comment at lines 106-117 — re-verified.
5. Destructor at line 198 — re-verified: `m_ValidationLayer = nullptr;` preserved (correct per v132 review §81-86).
6. v131+v135+v137+v138 patches intact in FGIPass.cpp (lines 579, 692, 301-318) + GIPathTracing.hlsl (lines 486-491).
7. v133 patch (NVRHI_WITH_VALIDATION force) intact at Engine/Source/Runtime/CMakeLists.txt:182.
8. v134 patch (validation TUs in add_library source list) intact at _deps/nvrhi-src/CMakeLists.txt:209-214.

## Audit summary

- **8 file-only tests re-run** in this audit. All PASS.
- **0 behavioral tests runnable** in this runspace (terminal + vision blocked by tirith per EC-039).
- **v139 patch is sound**: structural correctness re-verified by file inspection.
- **Pipeline state**: this is the LAST file-only cycle possible per the v139 plan + impl-review + tests. All 10 patches (v131-v139) are on disk. The pipeline is structurally complete. The parent runspace with terminal+vision must execute the build/run/VUID-inspection recipe.

## Cycle reasoning

The v139 patch is the v132 patch re-applied with one structural improvement (explicit `<nvrhi/validation.h>` include). The v132 patch failed at LINK time at tick 167 because the validation TUs were not in `add_library` source list (ninja dep-graph staleness). v133+v134 placed the validation TUs in `add_library` source list at tick 193, with a 12-line durability comment explaining the fix. v139 re-applies v132's patch now that the link prerequisite is satisfied.

If the parent runspace runs the recipe at v139 PENDING_TESTS_v139.md §"Behavioral tests":

1. Rebuild: `./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug`
   - **Expected**: SUCCESS. The v134 patch ensures the validation TUs are compiled into `libnvrhid.a`. The v139 patch's `createValidationLayer(m_NvrhiDevice)` call should resolve at link time.
   - **Fallback if FAIL**: revert the stub (re-apply `m_ValidationLayer = nullptr;` at line 118) and add a one-line log message about validation layer unavailability. The pipeline then continues with v138 discriminator chain (mode 6 → mode 20 → validate) without relying on VUID surfacing.

2. Run with default mode (validation layer enabled by CVar default `g_UseValidationLayers = true`):
   - `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal`
   - **If nvrhi validation messages fire**: this is the BISECT-CLOSING evidence. The actual Vulkan VUID names the GI shader's GBuffer SRV layout issue. v140 (or later) addresses the actual fix based on the VUID.
   - **If no nvrhi validation messages**: try `VK_LAYER_KHRONOS_validation=1` env var (forces system-level validation layer) and re-grep for VUID.

3. Run v138 discriminator chain:
   - `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=6 ./Binary/Debug/TestReSTIR_GI_Temporal`
   - **Per-pixel gradient visible**: v137 fixed the UAV bug. v131+v135 fixed the SRV read. Modes 20/21/22 should ALSO return non-zero. Run mode 20 to confirm.
   - **All-zero dump**: SRV bug is present. v137 was a wrong-fix for symptom. v140 (or later) investigates the SRV binding/layout.

4. If mode 6 shows gradient, confirm with mode 20:
   - `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 ./Binary/Debug/TestReSTIR_GI_Temporal`
   - **Non-zero per-pixel GBufferMaterial**: bisect closes. Run validator.
   - **All-zero**: SRV bug is present.

5. Validate:
   - `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`
   - **Expect**: 4/4 PASS if mode 20 returned non-zero.

6. Vision check on display PNG:
   - **Expect**: recognizable Sponza with sane exposure (not pure black, not all-white).

## Cycle verdict

**ALL_KEEP**: The patch is correct, the file-only deliverables are complete, no behavioral defects are detectable from file inspection, no broken-test patterns are present.

**The bisect cannot close in this file-only runspace.** Behavioral verification is the parent's responsibility per the trigger condition (a)/(c) policy (terminal access must be granted in this runspace OR the recipe must be executed from a parent runspace with terminal). The 7-criteria acceptance gate — debug target builds; HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8; no Vulkan VUID/ERROR; no command-list errors; validate_restir_gi.py passes newest dump group only; fresh display image (vision) shows recognizable Sponza with sane exposure; HLVM_PT_DEBUG_MODE=20 returns non-zero GBufferMaterial — is unreachable in this runspace.

## Honest blocker report

**Concrete external blocker**: terminal access is structurally blocked in this cron runspace (tirith "User denied this command" on every probe). The recipe at PENDING_TESTS_v139.md §"Behavioral tests" cannot be executed here. The pipeline has produced all 10 patches (v131-v139) — each verified by file inspection and per-role review — but cannot close the bisect without actually rebuilding and running the test target.

The pipeline dispatcher has done everything it can file-only. The work now requires a session with terminal access. Either:
- (a) Grant terminal access in this runspace (the `enabled_toolsets: ["terminal", "file"]` configuration per the six-role-pipeline §"File-only mode vs. terminal-enabled mode" and software-development-practices §"Subagent-Driven Development" anti-pattern #7).
- (b) Execute the recipe from a parent runspace with terminal + vision.
- (c) Pause the six-role cron and continue interactive debugging.

**No fabrication**: this audit does NOT claim the build succeeded, the binary ran, any dump was analyzed, the validation layer fired VUID, or the bisect is closed. The patches are correct on static analysis. The build/run/verification requires the parent runspace with terminal+vision.

---

**Per `six-role-pipeline §Role #6 (testing-verifier)`, this audit is file-only. Behavioral verification deferred to parent runspace per EC-039.**