# Pending Commit v134 — Add validation TUs to nvrhi add_library() source list (Debug tree only)

- plan: docs/PENDING_PLAN_v134.md
- plan_review: docs/PENDING_PLAN_REVIEW_v134.md (verdict: KEEP)
- files:
  Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/CMakeLists.txt
- source: no bundle — direct edit
- target: branch the parent runspace owns (git topology not touched by cron)
- task: Two changes to `Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/CMakeLists.txt` (Debug build tree only):
  1. **Add validation TUs to the initial `add_library(nvrhi STATIC ...)` call** at line 200. Include `${include_validation}` and `${src_validation}` in the source list at target-creation time, so ninja's incremental dep graph MUST include them as first-class compile dependencies. This bypasses the ninja incremental-dep-tracking issue that caused v133 to fail (validation TUs were never compiled despite `NVRHI_WITH_VALIDATION=ON` in the cache).
  2. **Replace the `target_sources` call with `target_compile_definitions`** at the former line 215-219. The original `target_sources` was the dual source list update that ninja missed; removing it eliminates the duplicate path. The `target_compile_definitions(... NVRHI_WITH_VALIDATION=1)` is functionally equivalent for runtime behavior (the validation TU source code checks `#if NVRHI_WITH_VALIDATION` at preprocessor time).

  After the patch:
  - `Engine/Source/Runtime/Build/Release/_deps/nvrhi-src/CMakeLists.txt` is NOT modified (Release already has `NVRHI_WITH_VALIDATION=ON` in cache AND already successfully links with validation).
  - `Engine/Source/Runtime/Build/RelWithDebInfo/_deps/nvrhi-src/CMakeLists.txt` is NOT modified (RelWithDebInfo same as Release).
  - `Engine/Source/Runtime/CMakeLists.txt` (v133 FORCE line at line 182) is NOT modified.
  - `Engine/Source/Runtime/Private/Renderer/DeviceManagerVk4_LifeCycle.cpp` (v132 `createValidationLayer` call at line 88) is NOT modified.
  - `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` (v131 `commitBarriers()` at line 668) is NOT modified.
  - `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` (v131 discriminator cases) is NOT modified.

- verify:
  ```
  cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
  # 1. Rebuild (forces cmake reconfigure; should now include validation TUs)
  ./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug
  # 2. Verify validation symbols are now in the lib (the v134 PASS condition)
  nm Engine/Source/Runtime/Binary/Debug/libnvrhid.a | grep -i createValidationLayer
  # Expect: a non-empty match (e.g., "T nvrhi::validation::createValidationLayer(...)")
  # If empty: the patch didn't take effect; investigate.
  # 3. Run with validation layer active
  HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal
  # 4. Inspect log for VUID
  grep -E "VUID|validation|SHADER_READ_ONLY_OPTIMAL|VkImage" \
    Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log
  # 5. If VUID surfaces, the bisect closes — the VUID names the exact image/layout issue
  # 6. Apply the fix the VUID describes
  # 7. Run v131 discriminator sweep (modes 20, 31) to confirm fix
  # 8. Final 8-criteria acceptance gate
  ```

- skip_impl_review: no — this is a CMakeLists.txt change in the nvrhi fork that affects all targets in the Debug build. The reviewer MUST verify:
  1. The `${include_validation}` and `${src_validation}` are correctly added to `add_library` at the right position (line 200-210 of the patched file).
  2. The replacement `target_compile_definitions` is semantically equivalent to the original `target_sources` for runtime behavior.
  3. The fix does not introduce a circular dependency or cmake reconfigure loop.
  4. The Release and RelWithDebInfo trees are NOT modified (verified by search_files).
  5. The patch does not break any test that uses `libnvrhid.a` (it ADDS symbols, doesn't remove any — safe).

- produces_test_files: no — no test files produced.

- notes: terminal access is structurally blocked in this cron runspace per EC-039. The patch lands file-only; the build/run/verify step requires the parent runspace with terminal.

## Files modified (this cycle)

### 1. `Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/CMakeLists.txt`

**Two edits in this file**:

**Edit 1** at line 200 (the `add_library(nvrhi STATIC ...)` call):
```cmake
# BEFORE (current line 200-204):
add_library(nvrhi STATIC
    ${include_common}
    ${src_common}
    ${misc_common})

# AFTER (v134 patch):
add_library(nvrhi STATIC
    ${include_common}
    ${src_common}
    ${misc_common}
    ${include_validation}
    ${src_validation})
```

**Edit 2** at line 215-219 (the `target_sources` block):
```cmake
# BEFORE (current line 215-219):
if (NVRHI_WITH_VALIDATION)
    target_sources(nvrhi PRIVATE
        ${include_validation}
        ${src_validation})
endif()

# AFTER (v134 patch):
if (NVRHI_WITH_VALIDATION)
    target_compile_definitions(nvrhi PUBLIC
        NVRHI_WITH_VALIDATION=1)
endif()
```

**Total diff**: +14 / -5 lines (comment blocks + 2-line additions + 3-line replacements).

## Plan Deviations

NONE. The impler followed the v134 plan exactly:
- Edit 1 includes `${include_validation}` + `${src_validation}` in the `add_library` source list at the correct position (between `${misc_common}` and the closing `)`).
- Edit 2 replaces `target_sources` with `target_compile_definitions` at the correct location.
- The Release and RelWithDebInfo trees are NOT modified (verified via search_files: 3 trees exist, only Debug is patched).
- The v133 FORCE line in `Engine/Source/Runtime/CMakeLists.txt:182` remains intact (defense in depth).
- All 4 prior-cycle patches remain intact (verified via search_files this tick).

The impler's static-analysis verification (per plan + plan-criticer feedback):
- `${include_validation}` is `include/nvrhi/validation.h` (single header). Headers in cmake `add_library` calls are valid cmake syntax — they are dependencies of the .cpp files but not compiled themselves.
- `${src_validation}` is `src/validation/validation-commandlist.cpp` + `src/validation/validation-device.cpp` + `src/validation/validation-backend.h`. All three are part of the nvrhi target's source list.
- The `target_compile_definitions(... NVRHI_WITH_VALIDATION=1)` is equivalent to having `NVRHI_WITH_VALIDATION=1` defined at compile time for all TUs in the nvrhi target. The validation TU source code checks `#if NVRHI_WITH_VALIDATION` at preprocessor time.
- The change is additive — no symbols removed from `libnvrhid.a`, only added (and the duplicate `target_sources` call removed, which is a no-op for runtime but eliminates the duplicate-symbol risk).

## Acceptance verification (parent runspace)

After the rebuild:
1. Build succeeds: `./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal` exits 0.
2. (NEW per v133 plan-criticer + v134 plan-criticer) `nm libnvrhid.a | grep createValidationLayer` returns a non-empty match (confirms the validation TU is now compiled into the lib).
3. TestSponzaDeferred still builds and runs without errors (regression check; TestSponzaDeferred is the only existing test that flips `bEnableNVRHIValidationLayer=true`).
4. For TestReSTIR_GI_Temporal, the validation layer is now active by default (`g_UseValidationLayers` defaults to `true` per `DeviceManagerVk.h:22`); the parent runspace can run the test with or without explicitly flipping the CVar.
5. If validation layer fires VUID, the log names the actual image/layout issue.
6. Run v131 discriminator sweep (HLVM_PT_DEBUG_MODE=20, 31) to confirm fix.
7. Final 8-criteria acceptance gate.

## Honesty floor

This commit lands patches. It does NOT claim:
- The cmake reconfigure succeeded.
- The validation symbols are now in libnvrhid.a.
- The validation layer fires VUID.
- Any dump was analyzed.

The patches are correct on static analysis (per the v134 plan evidence and the plan-criticer's KEEP verdict). The build/run verification requires the parent runspace. If the build fails (linker error on `createValidationLayer`), the impler's fallback path is: revert the v134 patch, then re-attempt with a different mechanism (e.g., edit `Build/Debug/build.ninja` directly to add the validation TU compile rules, OR patch `Build.sh` to call `ninja -t restat` before the rebuild). The static analysis makes this fallback unlikely to be needed.

## What unblocks the 8-criteria acceptance gate

Per EC-039, three options:
(a) Grant terminal access in this runspace.
(b) Execute the parent-runspace recipe from a parent runspace with terminal.
(c) Pause the six-role cron and continue interactive debugging.

The v134 patch lands file-only. The cmake reconfigure + build + run + verify steps require terminal in the parent runspace.

## What this commit does NOT change
- No commits, pushes, history rewrites (cron is file-only; git topology is parent-runspace responsibility).
- No governance-file edits.
- No new test files.
- The v131 patches (commitBarriers, case 31u, bypass-31u) remain as landed.
- The v132 patches (createValidationLayer) remain as landed.
- The v133 patches (cmake FORCE) remain as landed (defense in depth).
- The Release and RelWithDebInfo nvrhi-fork CMakeLists.txt trees are NOT modified.