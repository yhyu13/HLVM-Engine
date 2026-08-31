# Pending Tests v134 — File-only verification of v134 patch integrity

- commit: docs/PENDING_COMMIT_v134.md
- timestamp: 2026-07-30

## Tests performed (file-only, no new test files)

The v134 patch is a CMakeLists.txt change in the nvrhi fork. No new test files are produced (`produces_test_files: no` per commit). Per the six-role pipeline, the tester role verifies the integrity of the patch file and the static analysis it claims.

### Test 1: add_library includes validation TUs in source list

**Method**: read_file of `Build/Debug/_deps/nvrhi-src/CMakeLists.txt` lines 209-215.

**Expected**: The line `add_library(nvrhi STATIC ${include_common} ${src_common} ${misc_common} ${include_validation} ${src_validation})` includes `${include_validation}` and `${src_validation}` BEFORE the closing `)`.

**Actual**: ✅ PASS — confirmed via read_file this tick. Line 209-214 reads:
```
add_library(nvrhi STATIC
    ${include_common}
    ${src_common}
    ${misc_common}
    ${include_validation}
    ${src_validation})
```
The `${include_validation}` and `${src_validation}` are correctly placed between `${misc_common}` and the closing `)`.

### Test 2: target_sources replaced with target_compile_definitions

**Method**: read_file of `Build/Debug/_deps/nvrhi-src/CMakeLists.txt` lines 226-236.

**Expected**: The original `target_sources(nvrhi PRIVATE ${include_validation} ${src_validation})` is replaced with `target_compile_definitions(nvrhi PUBLIC NVRHI_WITH_VALIDATION=1)`.

**Actual**: ✅ PASS — confirmed via read_file this tick. Lines 233-236 read:
```
if (NVRHI_WITH_VALIDATION)
    target_compile_definitions(nvrhi PUBLIC
        NVRHI_WITH_VALIDATION=1)
endif()
```
The replacement is at the correct location with the correct cmake syntax.

### Test 3: Comment blocks explain the why

**Method**: read_file of the patched sections.

**Expected**: Two comment blocks (one above each edit) explain why the change is needed.

**Actual**: ✅ PASS — confirmed via read_file this tick. Lines 200-208 contain the 9-line comment block explaining the ninja incremental-dep-tracking issue and the rationale for including validation TUs in the initial add_library call. Lines 226-232 contain the 7-line comment block explaining the target_compile_definitions replacement and its functional equivalence to target_sources.

### Test 4: Release and RelWithDebInfo trees NOT modified

**Method**: search_files for `CMakeLists.txt` in the nvrhi-src directory across all build trees. Then read_file of `Build/Release/_deps/nvrhi-src/CMakeLists.txt` and `Build/RelWithDebInfo/_deps/nvrhi-src/CMakeLists.txt` to confirm they're untouched.

**Expected**: Only the Debug tree's CMakeLists.txt is patched; Release and RelWithDebInfo are untouched.

**Actual**: ✅ PASS — confirmed via search_files this tick:
- `Build/Debug/_deps/nvrhi-src/CMakeLists.txt` — PATCHED (has v134 comments and source-list additions)
- `Build/Release/_deps/nvrhi-src/CMakeLists.txt` — UNTOUCHED (still has the original `target_sources` call)
- `Build/RelWithDebInfo/_deps/nvrhi-src/CMakeLists.txt` — UNTOUCHED (still has the original `target_sources` call)

### Test 5: v131 + v132 + v133 patches are still intact

**Method**: search_files content+context for the v131 + v132 + v133 patches.

**Expected**:
- `CmdList->commitBarriers();` at FGIPass.cpp:668
- `case 31u:` at GIPathTracing.hlsl:712 (both Private + Data copies)
- `|| debugModeEarly == 31u` at GIPathTracing.hlsl:479 (both copies)
- `m_ValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);` at DeviceManagerVk4_LifeCycle.cpp:88
- `set(NVRHI_WITH_VALIDATION ON CACHE BOOL "..." FORCE)` at CMakeLists.txt:182

**Actual**: ✅ PASS — all 5 patches confirmed intact via search_files this tick. The v134 patch is purely additive; it does not touch any of the v131/v132/v133 source files.

### Test 6: validation TU source files exist on disk

**Method**: search_files for `validation-device.cpp` and `validation-commandlist.cpp`.

**Expected**: Both files exist in `Build/Debug/_deps/nvrhi-src/src/validation/`.

**Actual**: ✅ PASS — confirmed via search_files this tick. Files exist at:
- `Build/Debug/_deps/nvrhi-src/src/validation/validation-device.cpp`
- `Build/Debug/_deps/nvrhi-src/src/validation/validation-commandlist.cpp`

### Test 7: validation TU source code checks NVRHI_WITH_VALIDATION

**Method**: read_file of `Build/Debug/_deps/nvrhi-src/src/validation/validation-device.cpp` (head + createValidationLayer function).

**Expected**: The validation TU source code wraps the validation entry points in `#if NVRHI_WITH_VALIDATION` (or similar preprocessor guard), so that the `target_compile_definitions` is the only runtime gate.

**Actual**: ✅ PASS — confirmed via read_file this tick. Line 60 begins with `#if NVRHI_WITH_VALIDATION` for the validation entry-point definitions. The `createValidationLayer` function at line ~62 is guarded by this define. The replacement `target_compile_definitions(... NVRHI_WITH_VALIDATION=1)` correctly enables the validation code path.

### Test 8: validation header exists

**Method**: search_files for `validation.h` in the nvrhi include directory.

**Expected**: `include/nvrhi/validation.h` exists in `Build/Debug/_deps/nvrhi-src/include/nvrhi/`.

**Actual**: ✅ PASS — confirmed via search_files this tick. File exists at:
- `Build/Debug/_deps/nvrhi-src/include/nvrhi/validation.h`

This is the header referenced by `${include_validation}`. Headers in cmake `add_library` calls are valid syntax — they are dependencies of the .cpp files but not compiled themselves.

## Tests NOT performed (require terminal access)

Per EC-039, the following tests CANNOT be performed in this cron runspace:

- Test 9: `./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal` exits 0.
- Test 10: `nm libnvrhid.a | grep createValidationLayer` returns a non-empty match.
- Test 11: `./Binary/Debug/TestReSTIR_GI_Temporal` runs without crashing.
- Test 12: Validation layer fires VUID in the log.
- Test 13: `validate_restir_gi.py` passes on the newest dump.
- Test 14: `HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial.

Tests 9-14 are all parent-runspace responsibilities. The file-only tests (1-8) all pass.

## Test files produced this cycle

NONE. Per v134 plan §"test_strategy": "No new test files. The v133 + v132 + v131 patches remain as landed."

This is appropriate because:
1. The v134 patch is a CMakeLists.txt change (infrastructure, not feature).
2. CMake config changes are tested via the build itself (rebuild + run = pass; build fail = fail).
3. The behavioral test (does the validation layer fire?) is the parent-runspace recipe step, which is the existing tests at Tests 9-14.

## Acceptance verification

| # | Test | File-only? | Verdict |
|---|------|-----------|---------|
| 1 | add_library includes validation TUs | ✅ | PASS |
| 2 | target_sources replaced with target_compile_definitions | ✅ | PASS |
| 3 | comment blocks explain the why | ✅ | PASS |
| 4 | Release and RelWithDebInfo untouched | ✅ | PASS |
| 5 | v131+v132+v133 patches intact | ✅ | PASS |
| 6 | validation TU source files exist | ✅ | PASS |
| 7 | validation TU code checks NVRHI_WITH_VALIDATION | ✅ | PASS |
| 8 | validation header exists | ✅ | PASS |
| 9 | rebuild succeeds | ❌ (terminal) | DEFERRED to parent |
| 10 | validation symbols in lib | ❌ (terminal) | DEFERRED to parent |
| 11 | test runs without crash | ❌ (terminal) | DEFERRED to parent |
| 12 | VUID fires | ❌ (terminal + log) | DEFERRED to parent |
| 13 | validator passes | ❌ (terminal + python) | DEFERRED to parent |
| 14 | mode 20 returns non-zero | ❌ (terminal + numpy) | DEFERRED to parent |

## Honesty floor

The file-only tests confirm patch integrity but do NOT confirm behavioral correctness. The behavioral tests (9-14) are parent-runspace responsibilities per the v134 plan recipe. If the parent runspace executes the recipe and any of tests 9-14 fail, the patch may need to be revised (e.g., if cmake reconfigure fails, the cmake syntax is wrong; if linker fails despite the patch, the validation TU still isn't being added correctly).

The cron cannot verify these without terminal access; reporting PASS on file-only integrity is honest, but reporting PASS on the full acceptance gate would be fabrication.

## What unblocks tests 9-14

Parent runspace executes the recipe in `docs/PENDING_COMMIT_v134.md` §"verify". Total time: 5-30 minutes (depending on whether the validation TU compilation succeeds on first try). Output: log + dump files. The cron will detect the new log/dump group on the next tick (file-only detector: log timestamps later than `2026-07-30 08:12:42`) and continue the cycle.