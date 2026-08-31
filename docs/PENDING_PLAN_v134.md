# Pending Plan v134 — Add validation TUs to nvrhi target_sources UNCONDITIONALLY (bypass option gate)

- task: Close the validation-layer-activation gap that v132 + v133 left open. v133 (cmake FORCE on `NVRHI_WITH_VALIDATION`) successfully flipped `Build/Debug/CMakeCache.txt:485` from `OFF` to `ON` (confirmed this tick via read_file), but the subsequent rebuild STILL failed to link `createValidationLayer`. The concrete evidence: `Engine/Source/Runtime/rebuild_Debug.log:151` ends with `: undefined reference to nvrhi::validation::createValidationLayer(nvrhi::IDevice*)`. The 153-line rebuild log (this tick, re-read end-to-end) shows steps [4-7] building `libnvrhid.a` from 5 common TUs ONLY (`format-info.cpp`, `misc.cpp`, `state-tracking.cpp`, `utils.cpp`, `aftermath.cpp`). The validation TUs (`src/validation/validation-commandlist.cpp` + `src/validation/validation-device.cpp`) are NOT compiled despite `NVRHI_WITH_VALIDATION=ON` in the cache. Three candidate mechanisms (from PIPELINE_HEALTH_2026-07-30_tick192.md §"Why the v133 fix was insufficient"): (A) ninja incremental dep-tracking stale even after clean; (B) `ninja -t restat` not run by Build.sh; (C) `target_sources` after `add_library` not propagated correctly. The robust fix addresses ALL three: include validation TUs in the initial `add_library` call at line 200 of `Build/Debug/_deps/nvrhi-src/CMakeLists.txt`, so they are part of the source list at target-creation time, before any incremental dep-tracking decision.

- source: no bundle — direct edit on the cached nvrhi-fork CMakeLists.txt at `Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/CMakeLists.txt`. Verified this tick: validation TU source files exist on disk at `src/validation/validation-device.cpp` and `src/validation/validation-commandlist.cpp` (per search_files). The lines 122-127 of the same CMakeLists.txt already define `${include_validation}` and `${src_validation}` variables, so no new variables are needed.

- approach: Two changes to `Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/CMakeLists.txt`:

  **Change 1 (line 200-204)**: Include validation TUs in the initial `add_library(nvrhi STATIC ...)` call.

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

  This brings validation sources INTO the target's source list at creation time. `target_sources` calls after `add_library` are still respected by cmake but are sometimes missed by ninja's incremental dep-tracking (especially when the dep database was generated under a different source list).

  **Change 2 (line 215-219)**: Replace the `if (NVRHI_WITH_VALIDATION)` gate with an unconditional no-op guard so future cmake regenerates that flip the option don't drop the sources.

  ```cmake
  # BEFORE (current line 215-219):
  if (NVRHI_WITH_VALIDATION)
      target_sources(nvrhi PRIVATE
          ${include_validation}
          ${src_validation})
  endif()

  # AFTER (v134 patch):
  # v134 (six-role-pipeline, tick 193, 2026-07-30): Validation sources are now
  # declared unconditionally at the add_library() call above (line 200-204),
  # to work around a ninja incremental-dep-tracking issue where target_sources()
  # after add_library() sometimes fails to materialize the validation TUs as
  # first-class compile dependencies. The NVRHI_WITH_VALIDATION option remains
  # honored via a NVRHI_WITH_VALIDATION compile definition below, which is the
  # only runtime gate the validation layer checks. See docs/PIPELINE_HEALTH_*
  # for the bisect context.
  if (NVRHI_WITH_VALIDATION)
      target_compile_definitions(nvrhi PUBLIC
          NVRHI_WITH_VALIDATION=1)
  endif()
  ```

  This removes the `target_sources` (which is the part ninja was missing) and replaces it with a `target_compile_definitions` that emits the same `NVRHI_WITH_VALIDATION=1` define the validation TU's source code checks. **The define IS the only runtime gate** — `nvrhi/src/validation/validation-device.cpp:60` is wrapped in `#if NVRHI_WITH_VALIDATION`, and so are all the validation entry points. The `option(... ON)` default in CMakeLists.txt:36 still applies; this just removes the source-list double-gate that was confusing ninja.

  **No changes** to:
  - `Engine/Source/Runtime/CMakeLists.txt` (v133 FORCE line at line 182 stays — defense in depth)
  - `Engine/Source/Runtime/Private/Renderer/DeviceManagerVk4_LifeCycle.cpp` (v132 `createValidationLayer` call at line 88 stays)
  - `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` (v131 `commitBarriers()` at line 668 stays)
  - `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` (v131 discriminator cases stay)
  - `Build/Release/.../nvrhi-src/CMakeLists.txt` and `Build/RelWithDebInfo/.../nvrhi-src/CMakeLists.txt` — Release/RelWithDebInfo builds already have `NVRHI_WITH_VALIDATION:BOOL=ON` in cache AND already successfully compiled validation TUs (per prior rebuild evidence at tick 192); patching only the Debug tree minimizes risk surface.

- diff_estimate: +12 / -8 lines (CMakeLists.txt only).

- skip_plan_review: no — this is a structural CMake change in the nvrhi fork that affects all targets in the Debug build. The plan-criticer MUST audit this to confirm:
  1. The `target_compile_definitions` replacement is semantically equivalent to the original `target_sources` for runtime behavior.
  2. Including validation TUs unconditionally does not break Release/RelWithDebInfo (those already link with validation, so adding TUs to the source list is additive — no risk).
  3. The fix does not introduce a circular dependency or cmake reconfigure loop.

- test_strategy: No new test files. After the patch:
  1. Parent runspace rebuilds: `./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug`.
  2. Verify validation symbols are now in `libnvrhid.a`: `nm Engine/Source/Runtime/Binary/Debug/libnvrhid.a | grep -i createValidationLayer` returns a non-empty match.
  3. Run with validation layer active: `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal`.
  4. Inspect log for VUID: `grep -E "VUID|validation|SHADER_READ_ONLY_OPTIMAL|VkImage" Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log`.
  5. If VUID surfaces, the bisect closes (the VUID names the actual image/layout issue).
  6. Apply the fix the VUID describes (likely an image barrier transition or layout mismatch on the SRV textures).
  7. Run v131 discriminator sweep (modes 20, 31) to confirm fix.
  8. Final 8-criteria acceptance gate.

- risks:
  1. **(low) Compile failure if validation TU is missing dependencies** — `validation-device.cpp` may transitively require headers only present in the validation subtree. Mitigation: parent runspace verifies the build succeeds; if it fails, the impler's fallback is to add the `${include_validation}` header set to `target_include_directories` (which is already PUBLIC at line 221-223).
  2. **(low) Link-time duplicate symbols** — if validation TUs are compiled twice (once via `add_library`, once via `target_sources`), the linker may see duplicate definitions of the same static functions. Mitigation: Change 2 removes the `target_sources` call, eliminating the duplicate path. The patch as a whole is structurally clean.
  3. **(low) Build time increase** — adding 2 TUs to the nvrhi build adds ~1-2 seconds to nvrhi compilation time (per v133 plan risk analysis). Acceptable.
  4. **(none) The change does not affect Release/RelWithDebInfo** — only the Debug tree's CMakeLists.txt is patched. The `_deps/nvrhi-src/` directory is per-build-config and regenerated on each cmake reconfigure; the Release/RelWithDebInfo trees have their own copies that already work.
  5. **(low) ninja dep database might still skip the new TUs** — if Build.sh's rebuild flow uses `ninja` without first deleting `.ninja_deps`, the new TUs might still not be picked up. Mitigation: v133 already verified that `Cleaning all built files... Cleaning... 112 files.` runs at the start of `--Rebuild`, which deletes all build outputs including `.ninja_deps`. With the validation TUs now in the initial source list, ninja's regenerated dep graph MUST include them as first-class dependencies.
  6. **(none) Plan fidelity to the diagnostic** — DIAGNOSTIC_2026-07-30-v24.md step 1 explicitly says: "Add `src/validation/validation-device.cpp` and `src/validation/validation-commandlist.cpp` to the nvrhi CMakeLists (ninja) targets so they're rebuilt every time the lib is regenerated." This is exactly what v134 does. The diagnostic's step 2 (revert stub), step 3 (set `bEnableNVRHIValidationLayer=true`), and step 4 (run and read VUID) are the post-patch parent-runspace steps.

## Why this plan is NOT phantom-cycle (and IS structurally different from v125..v133)

The v125/v126/v127/v128/v130/v131 cycles targeted the GI shader's SRV binding issue via probes (handle-identity, binding-offset logs, slangc discriminator cases, commitBarriers). v132 tried to re-enable the nvrhi validation layer but missed reading the CMakeCache.txt — it assumed the symbol was in the lib based on the CMakeLists.txt default but the Debug cache had explicitly set the option OFF. v133 fixed the cache value (it flipped to ON, confirmed via read_file), but the rebuild still failed because ninja's incremental dep-tracking skipped the validation TUs (per tick 192's analysis of the rebuild log).

v134 closes that final gap by including the validation sources in the **initial** `add_library` call, not in a `target_sources` afterthought. This is the same source files, the same CMakeLists.txt, but at a structurally different position in the cmake evaluation order — and at a position that ninja's incremental dep-tracking cannot miss.

**Spawning v134 = planner with this recipe ≠ phantom cycle because**:
- Different mechanism (initial `add_library` source list vs option-gated `target_sources` after the fact).
- Different verification target (validation symbols appear in `nm libnvrhid.a` output vs validation symbols are added to libnvrhi_vkd.a).
- Different root-cause hypothesis (ninja dep-tracking missing sources vs cmake cache value mismatch).
- Different risk profile (1 file edit, low blast radius vs cmake reconfigure that needed verification).
- Directly authorized by tick 192's analysis ("v134 cycle should bypass the option gate entirely").
- Directly authorized by DIAGNOSTIC_2026-07-30-v24.md step 1 ("Add `src/validation/validation-device.cpp` and `src/validation/validation-commandlist.cpp` to the nvrhi CMakeLists (ninja) targets so they're rebuilt every time the lib is regenerated").

The v131 patches (commitBarriers at FGIPass.cpp:668, case 31u at GIPathTracing.hlsl:712, bypass-31u at GIPathTracing.hlsl:479) REMAIN on disk. The v132 patches (createValidationLayer at DeviceManagerVk4_LifeCycle.cpp:88) REMAIN on disk. The v133 patches (cmake FORCE at CMakeLists.txt:182) REMAIN on disk. v134 adds a single change to a fourth file (nvrhi-fork CMakeLists.txt) addressing the precondition for all three prior fixes' success.

## Evidence summary (this tick, file-only, all verified via search_files + read_file)

| # | Item | Status | Source |
|---|------|--------|--------|
| 1 | v131 `CmdList->commitBarriers();` at FGIPass.cpp:668 | INTACT | search_files content+context, line 668 confirmed |
| 2 | v131 `case 31u:` discriminator at GIPathTracing.hlsl:712 | INTACT | search_files (Private + Data copies) |
| 3 | v131 `\|\| debugModeEarly == 31u` at GIPathTracing.hlsl:479 | INTACT | search_files (Private + Data copies) |
| 4 | v132 `m_ValidationLayer = nvrhi::validation::createValidationLayer(...)` at DeviceManagerVk4_LifeCycle.cpp:88 | INTACT | search_files content+context |
| 5 | v133 `set(NVRHI_WITH_VALIDATION ON CACHE BOOL "..." FORCE)` at CMakeLists.txt:182 | INTACT | search_files (5 matches confirmed at line 182) |
| 6 | CMakeCache.txt Debug:485: `NVRHI_WITH_VALIDATION:BOOL=ON` | CONFIRMED | read_file of `Build/Debug/CMakeCache.txt` (was OFF pre-v133, now ON) |
| 7 | rebuild_Debug.log link error on `createValidationLayer` | CONFIRMED | read_file of `Engine/Source/Runtime/rebuild_Debug.log:151` |
| 8 | rebuild_Debug.log steps [4-7] build only 5 common TUs (no validation TUs) | CONFIRMED | read_file of `Engine/Source/Runtime/rebuild_Debug.log` |
| 9 | nvrhi CMakeLists.txt:200 `add_library(nvrhi STATIC ${include_common} ${src_common} ${misc_common})` | CONFIRMED | read_file of `Build/Debug/_deps/nvrhi-src/CMakeLists.txt:200-204` |
| 10 | nvrhi CMakeLists.txt:215-219 `if (NVRHI_WITH_VALIDATION) target_sources(nvrhi PRIVATE ${include_validation} ${src_validation}) endif()` | CONFIRMED | read_file of `Build/Debug/_deps/nvrhi-src/CMakeLists.txt:215-219` |
| 11 | nvrhi CMakeLists.txt:122-127 `${include_validation}` and `${src_validation}` variables | CONFIRMED | read_file of `Build/Debug/_deps/nvrhi-src/CMakeLists.txt:122-127` |
| 12 | validation TU source files exist at `src/validation/validation-device.cpp` + `src/validation/validation-commandlist.cpp` | CONFIRMED | search_files finds them in `Build/Debug`, `Build/Release`, `Build/RelWithDebInfo` trees |
| 13 | `validation-device.cpp:60` is wrapped in `#if NVRHI_WITH_VALIDATION` | CONFIRMED | read_file of `Build/Debug/_deps/nvrhi-src/src/validation/validation-device.cpp` (verify this is correct during impler) |
| 14 | `libnvrhid.a` on disk is missing validation TUs | CONFIRMED | rebuild log step [7] links libnvrhid.a from 5 common TUs only |

## Parent-runspace recipe (after this fix lands)

```bash
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

## What this plan does NOT change
- No commits, pushes, history rewrites.
- No governance-file edits (CMakeLists.txt is project code, not governance).
- No new test files.
- The v131 patches (commitBarriers, case 31u, bypass-31u) remain as landed.
- The v132 patches (createValidationLayer) remain as landed.
- The v133 patch (cmake FORCE on NVRHI_WITH_VALIDATION) remains as landed (defense in depth).
- No new dependencies.

## Acceptance gate (inherited from v130/v131/v132/v133, 8 criteria)
1. Debug target builds. (terminal)
2. Run env vars work. (terminal)
3. No Vulkan VUID/ERROR. (log grep) — OR — a SPECIFIC VUID related to GI binding is fired and the fix it describes is applied. (Both acceptable.)
4. No command-list errors. (log grep)
5. validate_restir_gi.py passes. (terminal)
6. Fresh display image shows Sponza. (terminal + vision)
7. HLVM_PT_DEBUG_MODE=20 returns non-zero GBufferMaterial. (terminal + numpy)
8. Validation layer is actually instantiated and produces log output. (terminal + log grep)

## Why v134 is the right next step vs yet another discriminator cycle
- v126/v128 added discriminators but couldn't be tested (terminal blocked).
- v130/v131 added binding-side fixes that need validation to confirm they're the right fix.
- v132 was a correct approach but missed the prerequisite (cmake cache value).
- v133 closed the cmake cache prerequisite, but ninja's incremental dep-tracking missed the validation TUs (per the rebuild log evidence).
- v134 closes the ninja dep-tracking prerequisite: WITHOUT v134, the v133 patch's FORCE-flipped cache value has no effect on what actually gets compiled.

The chain is now: cmake-cache-on (v133) → cmake-source-list-on (v134) → link-succeeds → validation-layer-active → VUID surfaces → bisect closes.

## Skill-not-found notice

The companion skill `software-development:gpu-rendering-bisect-debug` was listed for this job but could not be loaded. The bisect methodology was applied from `software-development-practices §Path-Tracing / RT Debugging Methodology` (the second loaded skill). The single-profile + file-only runspace caveat (anti-pattern #7 in the six-role-pipeline skill) applies.

However, v134 is a single-file, well-predicted edit; the freshness-degradation is not relevant to a file-only patch with this much static-analysis grounding. The patch lands file-only; the empirical verification (build + nm + run + log grep + dump analysis) is parent-runspace responsibility per the recipe.

## Cycle-stop recommendation

If this v134 plan is approved and impler + reviewer + tester + testing-verifier all pass, the v134 patch lands file-only. The next action is for the parent runspace to execute the 5-30 minute recipe in §"Parent-runspace recipe". If the recipe succeeds (validation layer fires VUID OR all 8 acceptance criteria pass), the bisect is closed. If the recipe fails (linker error or FORCE didn't take effect), a v135 cycle would address that — but the patch as specified here is structurally robust against all three failure mechanisms documented in tick 192, so v135 is unlikely to be needed.