# Pending Tests v133 — File-only verification of v133 patch integrity

- commit: docs/PENDING_COMMIT_v133.md
- timestamp: 2026-07-30

## Tests performed (file-only, no new test files)

The v133 patch is a CMakeLists.txt change. No new test files are produced (`produces_test_files: no` per commit). Per the six-role pipeline, the tester role verifies the integrity of the patch file and the static analysis it claims.

### Test 1: FORCE line is correctly placed BEFORE FetchContent_MakeAvailable

**Method**: read_file Engine/Source/Runtime/CMakeLists.txt with offset=169, limit=30.

**Expected**: The line `set(NVRHI_WITH_VALIDATION ON CACHE BOOL "..." FORCE)` appears BEFORE `FetchContent_MakeAvailable(nvrhi)`.

**Actual**: ✅ PASS — confirmed via read_file this tick. The FORCE line is at offset 182 (relative to file start), and `FetchContent_MakeAvailable(nvrhi)` is at offset 193.

### Test 2: cache var name is exactly NVRHI_WITH_VALIDATION

**Method**: search_files content+context on the patched file.

**Expected**: The string `NVRHI_WITH_VALIDATION` appears with `ON` and `FORCE` and matches the spelling in nvrhi/CMakeLists.txt:36.

**Actual**: ✅ PASS — confirmed via search_files. Exact case match. CMake cache vars are case-sensitive on Linux.

### Test 3: Comment block explains the why

**Method**: read_file of the patched section.

**Expected**: A multi-line comment explains why this change is needed (cmake cache default vs actual cache state).

**Actual**: ✅ PASS — 12-line comment block referencing DIAGNOSTIC_2026-07-30-v24.md, Build/Debug/CMakeCache.txt:485, and the v131/v132 history. Future agents can trace the rationale.

### Test 4: No other changes in the file

**Method**: read_file + manual diff against expected unchanged lines.

**Expected**: Only the v133 FORCE block is added; no other lines are modified.

**Actual**: ✅ PASS — patch tool output showed a clean +12 / -0 diff at lines 172-183.

### Test 5: v131 + v132 patches are still intact

**Method**: search_files content+context for the v131 + v132 patches.

**Expected**:
- `CmdList->commitBarriers();` at FGIPass.cpp:668
- `case 31u:` at GIPathTracing.hlsl:712 (both Private + Data copies)
- `|| debugModeEarly == 31u` at GIPathTracing.hlsl:479 (both copies)
- `m_ValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);` at DeviceManagerVk4_LifeCycle.cpp:88

**Actual**: ✅ PASS — all four patches confirmed intact via search_files this tick. The v133 patch is purely additive; it does not touch any of the v131/v132 source files.

## Tests NOT performed (require terminal access)

Per EC-039, the following tests CANNOT be performed in this cron runspace:

- Test 6: `./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal` exits 0.
- Test 7: `nm libnvrhi_vkd.a | grep createValidationLayer` returns a non-empty match.
- Test 8: `./Binary/Debug/TestReSTIR_GI_Temporal` runs without crashing.
- Test 9: Validation layer fires VUID in the log.
- Test 10: `validate_restir_gi.py` passes on the newest dump.
- Test 11: `HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial.

Tests 6-11 are all parent-runspace responsibilities. The file-only tests (1-5) all pass.

## Test files produced this cycle

NONE. Per v133 plan §"test_strategy": "No new test files. The v132 + v131 patches remain as landed."

This is appropriate because:
1. The v133 patch is a CMakeLists.txt change (infrastructure, not feature).
2. CMake config changes are tested via the build itself (rebuild + run = pass; build fail = fail).
3. The behavioral test (does the validation layer fire?) is the parent-runspace recipe step, which is the existing tests at Tests 6-11.

## Acceptance verification

| # | Test | File-only? | Verdict |
|---|------|-----------|---------|
| 1 | FORCE line placement | ✅ | PASS |
| 2 | cache var spelling | ✅ | PASS |
| 3 | comment block | ✅ | PASS |
| 4 | no other changes | ✅ | PASS |
| 5 | v131+v132 patches intact | ✅ | PASS |
| 6 | rebuild succeeds | ❌ (terminal) | DEFERRED to parent |
| 7 | validation symbols in lib | ❌ (terminal) | DEFERRED to parent |
| 8 | test runs without crash | ❌ (terminal) | DEFERRED to parent |
| 9 | VUID fires | ❌ (terminal + log) | DEFERRED to parent |
| 10 | validator passes | ❌ (terminal + python) | DEFERRED to parent |
| 11 | mode 20 returns non-zero | ❌ (terminal + numpy) | DEFERRED to parent |

## Honesty floor

The file-only tests confirm patch integrity but do NOT confirm behavioral correctness. The behavioral tests (6-11) are parent-runspace responsibilities per the v133 plan recipe. If the parent runspace executes the recipe and any of tests 6-11 fail, the patch may need to be revised (e.g., if cmake reconfigure fails, the FORCE keyword syntax is wrong; if linker fails despite FORCE, the validation TU still isn't being added correctly). The cron cannot verify these without terminal access; reporting PASS on file-only integrity is honest, but reporting PASS on the full acceptance gate would be fabrication.

## What unblocks tests 6-11

Parent runspace executes the recipe in `docs/PENDING_COMMIT_v133.md` §"verify". Total time: 60-180 seconds. Output: log + dump files. The cron will detect the new log/dump group on the next tick (file-only detector: log timestamps later than `2026-07-30 08:12:42`) and continue the cycle.