# Pending Commit v133 — Force-enable NVRHI_WITH_VALIDATION for all build configs

- plan: docs/PENDING_PLAN_v133.md
- plan_review: docs/PENDING_PLAN_REVIEW_v133.md (verdict: KEEP)
- files:
  Engine/Source/Runtime/CMakeLists.txt
- source: no bundle — direct edit
- target: branch the parent runspace owns (git topology not touched by cron)
- task: Add `set(NVRHI_WITH_VALIDATION ON CACHE BOOL "Build NVRHI the validation layer" FORCE)` to `Engine/Source/Runtime/CMakeLists.txt` BEFORE `FetchContent_MakeAvailable(nvrhi)` at line 181. This forces the cmake cache to set `NVRHI_WITH_VALIDATION=ON` regardless of any stale cache value (the current Debug cache has it OFF at `Build/Debug/CMakeCache.txt:485`). On the next cmake reconfigure (triggered by Build.sh), the validation TUs (`validation-device.cpp` + `validation-commandlist.cpp`) will be compiled into `libnvrhi_vkd.a`, making the v132 patch at `DeviceManagerVk4_LifeCycle.cpp:88` link successfully.

- verify:
  ```
  cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
  # 1. Rebuild (forces cmake reconfigure)
  ./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug
  # 1.5. (per plan-criticer suggestion) Verify validation symbols are now in the lib
  nm Engine/Source/Runtime/Binary/Debug/libnvrhi_vkd.a | grep -i createValidationLayer
  # Expect: a non-empty match (e.g., "T nvrhi::validation::createValidationLayer(...)")
  # If empty: the FORCE didn't take effect; investigate.
  # 2. Run with validation layer active
  HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal
  # 3. Inspect log for VUID
  grep -E "VUID|validation|SHADER_READ_ONLY_OPTIMAL|VkImage" \
    Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log
  # 4. If VUID surfaces, the bisect closes — the VUID names the exact image/layout issue
  # 5. Apply the fix the VUID describes
  # 6. Run v131 discriminator sweep (modes 20, 31)
  # 7. Final 7-criteria acceptance gate
  ```

- skip_impl_review: no — this is a CMakeLists.txt change that affects all build configurations. The reviewer MUST verify:
  1. The `FORCE` keyword is correctly placed BEFORE `FetchContent_MakeAvailable(nvrhi)` (not after — FORCE after MakeAvailable has no effect on the already-configured target).
  2. The cache var name matches the nvrhi CMakeLists.txt: `NVRHI_WITH_VALIDATION` (case-sensitive, exact match).
  3. The fix does not introduce a circular dependency or cmake reconfigure loop.

- produces_test_files: no — no test files produced.

- notes: terminal access is structurally blocked in this cron runspace per EC-039. The patch lands file-only; the build/run/verify step requires the parent runspace with terminal.

## Files modified (this cycle)

### 1. Engine/Source/Runtime/CMakeLists.txt

**Single-line edit** at line 172 (BEFORE `FetchContent_Declare(nvrhi)`):
```cmake
# v133 comment block (12 lines) + set(NVRHI_WITH_VALIDATION ON CACHE BOOL "..." FORCE)
```

Total diff: +12 / -0 lines (comment + the FORCE line).

## Plan Deviations

NONE. The impler followed the v133 plan exactly:
- Single FORCE line added before `FetchContent_MakeAvailable(nvrhi)`.
- Comment block added explaining why.
- No additional changes (the plan-criticer's suggested nm verification step is in the parent recipe, not in the patch).

The impler's static-analysis verification (per plan + plan-criticer feedback):
- `set(NVRHI_WITH_VALIDATION ON CACHE BOOL "" FORCE)` overrides any cached value.
- Placement BEFORE `FetchContent_MakeAvailable` ensures the override happens before nvrhi is configured (the FORCE keyword does affect subsequent `option()` calls if the variable is in the cache).
- The cmake reconfigure triggered by `Build.sh` will pick up the new value.
- Release and RelWithDebInfo already have `NVRHI_WITH_VALIDATION:BOOL=ON` in their caches; the FORCE is a no-op for them (they keep `=ON`).

## Acceptance verification (parent runspace)

After the rebuild:
1. Build succeeds: `./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal` exits 0.
2. (NEW per plan-criticer) `nm libnvrhi_vkd.a | grep createValidationLayer` returns a non-empty match (confirms the FORCE took effect).
3. TestSponzaDeferred still builds and runs without errors (regression check; TestSponzaDeferred is the only existing test that flips `bEnableNVRHIValidationLayer=true`).
4. For TestReSTIR_GI_Temporal, the validation layer is now active by default (`g_UseValidationLayers` defaults to `true` per `DeviceManagerVk.h:22`); the parent runspace can run the test with or without explicitly flipping the CVar.
5. If validation layer fires VUID, the log names the actual image/layout issue.
6. Run v131 discriminator sweep (HLVM_PT_DEBUG_MODE=20, 31) to confirm fix.
7. Final 7-criteria acceptance gate.

## Honesty floor

This commit lands patches. It does NOT claim:
- The cmake reconfigure succeeded.
- The validation symbols are now in libnvrhi_vkd.a.
- The validation layer fires VUID.
- Any dump was analyzed.

The patches are correct on static analysis (per the v133 plan evidence). The build/run verification requires the parent runspace. If the build fails (linker error on `createValidationLayer`), the impler's fallback path is: revert the FORCE line in CMakeLists.txt and the v132 stub in DeviceManagerVk4_LifeCycle.cpp, then re-attempt with a different mechanism (e.g., add validation TUs directly to libnvrhi_vkd.a via custom command). The static analysis makes this fallback unlikely to be needed (the FORCE keyword is well-defined cmake syntax).

## What unblocks the 7-criteria acceptance gate

Per EC-039, three options:
(a) Grant terminal access in this runspace.
(b) Execute the parent-runspace recipe from a parent runspace with terminal.
(c) Pause the six-role cron and continue interactive debugging.

The v133 patch lands file-only. The cmake reconfigure + build + run + verify steps require terminal in the parent runspace.

## What this commit does NOT change
- No commits, pushes, history rewrites (cron is file-only; git topology is parent-runspace responsibility).
- No governance-file edits (CMakeLists.txt is project code, not governance).
- No new test files.
- The v131 patches (commitBarriers, case 31u, bypass-31u) remain as landed.
- The v132 patches (createValidationLayer) remain as landed.
- The validation layer is still gated by `bEnableNVRHIValidationLayer` (default false via DeviceParams, but `|= g_UseValidationLayers` flips it on at runtime; the CVar defaults to true so the runtime path is exercised).
- No new dependencies.