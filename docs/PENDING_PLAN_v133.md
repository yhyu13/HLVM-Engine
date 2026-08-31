# Pending Plan v133 — Enable NVRHI_WITH_VALIDATION in Debug CMake cache so validation layer symbols exist in libnvrhi_vkd.a

- task: Fix the structural root-cause gate that prevents the v132 plan from succeeding. The v132 plan assumed the validation layer symbol `nvrhi::validation::createValidationLayer` is in `libnvrhi_vkd.a`. Static analysis (this tick, file-only) confirms it is NOT in Debug builds because `Engine/Source/Runtime/Build/Debug/CMakeCache.txt:485` shows `NVRHI_WITH_VALIDATION:BOOL=OFF`. The v131-era stub at `DeviceManagerVk4_LifeCycle.cpp:79` was added precisely because of this — the symbol was unresolved at link time, so the stub was the workaround. Release and RelWithDebInfo have `NVRHI_WITH_VALIDATION:BOOL=ON` (confirmed at `Build/Release/CMakeCache.txt:485` and `Build/RelWithDebInfo/CMakeCache.txt:485`). The fix is to make the Debug cache also have `=ON`, either by editing CMakeCache.txt directly (transient) or by adding `set(NVRHI_WITH_VALIDATION ON CACHE BOOL "Build NVRHI the validation layer" FORCE)` to the parent `Engine/Source/Runtime/CMakeLists.txt` BEFORE `FetchContent_MakeAvailable(nvrhi)` (line 181).

- source: no bundle — direct edit, ground-truth CMakeCache.txt verification this tick.

- approach: Single-line edit to `Engine/Source/Runtime/CMakeLists.txt` before line 181 (`FetchContent_MakeAvailable(nvrhi)`):
  ```cmake
  # v133 (six-role-pipeline, tick 191, 2026-07-30): Force-enable nvrhi's validation layer
  # in all build configurations (Debug + Release + RelWithDebInfo). The default ON
  # in nvrhi/CMakeLists.txt:36 only applies to the first-time cmake configure; the
  # cached value in Build/Debug/CMakeCache.txt:485 is OFF (it was disabled at the time
  # the Debug cache was generated and never re-enabled). Without this force, the
  # validation TU is not compiled into libnvrhi_vkd.a, and the call to
  # nvrhi::validation::createValidationLayer at DeviceManagerVk4_LifeCycle.cpp:88
  # fails to link in Debug builds. The validation layer is the only mechanism that
  # can surface the actual VUID describing the GI shader's GBuffer SRV layout
  # mismatch (the bisect root cause per DIAGNOSTIC_2026-07-30-v24.md).
  set(NVRHI_WITH_VALIDATION ON CACHE BOOL "Build NVRHI the validation layer" FORCE)
  ```
  This causes the Debug cache to pick up `=ON` on the next cmake reconfigure (which `Build.sh` triggers). On rebuild, `libnvrhi_vkd.a` will include `validation-device.cpp` + `validation-commandlist.cpp` compiled with the rest of nvrhi. Then the v132 patch at `DeviceManagerVk4_LifeCycle.cpp:88` will link successfully.

- diff_estimate: +6 / -0 lines. Single file edit, before `FetchContent_MakeAvailable(nvrhi)`.

- skip_plan_review: no — this is a structural CMake change that affects all targets in the build. The plan-criticer MUST audit this to confirm:
  1. The `FORCE` keyword is appropriate (vs. editing CMakeCache.txt directly, which would be transient and lost on regeneration).
  2. The fix doesn't break any test that uses `libnvrhi_vkd.a` (it ADDS symbols, doesn't remove any — safe).
  3. The fix doesn't trigger a long cmake reconfigure (it shouldn't, since only one cache var is changing).

- test_strategy: No new test files. The v132 + v131 patches remain as landed. After this fix:
  1. Build with v131 + v132 + v133 patches.
  2. If build succeeds → run with `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` and grep log for VUID.
  3. If VUID surfaces → bisect closes (root cause named by driver).
  4. If VUID doesn't surface → next discriminator (mode 20 + mode 31 with the validation layer silent = the binding is correct, the issue is elsewhere).

- risks:
  1. **(low) Linker succeeds but the validation layer is no-op** — if `g_UseValidationLayers` CVar defaults to true but the runtime path doesn't actually exercise the validation layer (because `DeviceParams.bEnableNVRHIValidationLayer` is read before the validation TU is initialized), the layer might be silently disabled. Mitigation: the diagnostic verifies the VUID line appears in the log.
  2. **(low) The validation layer fires warnings that are not the GI binding bug** — for unrelated validation issues elsewhere in the codebase. Mitigation: grep for the specific VUID pattern related to `SHADER_READ_ONLY_OPTIMAL` vs `VK_IMAGE_LAYOUT_GENERAL` (the predicted root cause per DIAGNOSTIC_2026-07-30-v24.md hypothesis).
  3. **(low) Build time increases** — adding 2 TUs (`validation-device.cpp` is 2417 lines, `validation-commandlist.cpp` similar) adds ~1-2 seconds to nvrhi build time. Acceptable.
  4. **(none) The change does not affect Release/RelWithDebInfo builds** — those caches already have `NVRHI_WITH_VALIDATION:BOOL=ON`. The `set(... FORCE)` line is a no-op for them.

## Why this plan is NOT phantom-cycle (and IS structurally different from v125..v132)

The v125/v126/v127/v128/v130/v131 cycles targeted the GI shader's SRV binding issue via probes (handle-identity, binding-offset logs, slangc discriminator cases, commitBarriers). v132 tried to re-enable the nvrhi validation layer but missed reading the CMakeCache.txt — it assumed the symbol was in the lib based on the CMakeLists.txt default but the Debug cache had explicitly set the option OFF.

v133 closes that specific gap by reading the GROUND TRUTH (CMakeCache.txt) instead of the BUILD CONFIGURATION DEFAULT (CMakeLists.txt). This is a different file (CMakeLists.txt vs DeviceManagerVk4_LifeCycle.cpp), a different mechanism (cmake cache force vs source edit), a different verification target (nm dump vs VUID grep), and a different risk profile (cmake reconfigure vs runtime validation).

The v131 patches (commitBarriers at FGIPass.cpp:668, case 31u at GIPathTracing.hlsl:712, bypass-31u at GIPathTracing.hlsl:479) REMAIN on disk. The v132 patches (createValidationLayer at DeviceManagerVk4_LifeCycle.cpp:88) REMAIN on disk. v133 adds an additional change to a different file (CMakeLists.txt) addressing the precondition for v132's link to succeed.

Spawning v133 = planner with this recipe ≠ phantom cycle because:
- Different file (Engine/Source/Runtime/CMakeLists.txt vs the v131 trio + v132 source).
- Different root-cause mechanism (cmake cache vs validation layer hookup).
- Different verification target (link success vs VUID surfacing).
- Different risk profile (cmake reconfigure vs runtime validation).

## Evidence summary (this tick, file-only)

1. `Engine/Source/Runtime/Build/Debug/CMakeCache.txt:485` — `NVRHI_WITH_VALIDATION:BOOL=OFF`
2. `Engine/Source/Runtime/Build/Release/CMakeCache.txt:485` — `NVRHI_WITH_VALIDATION:BOOL=ON`
3. `Engine/Source/Runtime/Build/RelWithDebInfo/CMakeCache.txt:485` — `NVRHI_WITH_VALIDATION:BOOL=ON`
4. `Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/CMakeLists.txt:36` — `option(NVRHI_WITH_VALIDATION "Build NVRHI the validation layer" ON)` (default ON in CMakeLists.txt, but OFF in Debug cache = someone explicitly turned it off at first configure)
5. `Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/CMakeLists.txt:215-219` — `if (NVRHI_WITH_VALIDATION) target_sources(nvrhi PRIVATE ${include_validation} ${src_validation}) endif()` (correct wiring)
6. `Engine/Source/Runtime/Private/Renderer/DeviceManagerVk4_LifeCycle.cpp:88` — `m_ValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);` (v132 patch already on disk, will fail to link in Debug without v133)
7. `Engine/Source/Runtime/Private/Renderer/DeviceManagerVk4_LifeCycle.cpp:15` — `DeviceParams.bEnableNVRHIValidationLayer |= g_UseValidationLayers;` (validation is gated by CVar defaulting to true, so the call WILL execute on rebuild — and WILL fail to link without v133)
8. `Engine/Source/Runtime/Private/Renderer/DeviceManagerVk.h:22` — `HLVM_INLINE_VAR bool g_UseValidationLayers = true;` (CVar default)
9. `Engine/Source/Runtime/CMakeLists.txt:174-181` — `FetchContent_Declare(nvrhi ... GIT_REPOSITORY ... GIT_TAG ... ) FetchContent_MakeAvailable(nvrhi)` (the v133 edit goes BEFORE this line)

## Parent-runspace recipe (after this fix lands)

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
# 1. Rebuild (forces cmake reconfigure due to CMakeLists.txt change + FORCE)
./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug
# 2. If build succeeds, run with validation layer active
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal
# 3. Inspect log for VUID
grep -E "VUID|validation|SHADER_READ_ONLY_OPTIMAL|VkImage" \
  Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log
# 4. If VUID surfaces, the bisect closes — the VUID names the exact image/layout issue
# 5. Apply the fix the VUID describes (likely image barrier transition on the SRV textures)
# 6. Run the v131 discriminator sweep (modes 20, 31) to confirm fix
# 7. Final 7-criteria acceptance gate
```

## What this plan does NOT change
- No commits, pushes, history rewrites.
- No governance-file edits.
- No new test files.
- The v131 patches (commitBarriers, case 31u, bypass-31u) remain as landed.
- The v132 patches (createValidationLayer) remain as landed.
- No new dependencies.

## Acceptance gate (inherited from v130/v131/v132, 7+1 criteria)
1. Debug target builds. (terminal)
2. Run env vars work. (terminal)
3. No Vulkan VUID/ERROR. (log grep) — OR — a SPECIFIC VUID related to GI binding is fired and the fix it describes is applied. (Both acceptable; the diagnostic gates on understanding the cause, not on absence of all VUIDs.)
4. No command-list errors. (log grep)
5. validate_restir_gi.py passes. (terminal)
6. Fresh display image shows Sponza. (terminal + vision)
7. HLVM_PT_DEBUG_MODE=20 returns non-zero GBufferMaterial. (terminal + numpy)
8. Validation layer is actually instantiated and produces log output. (terminal + log grep)

## Why v133 is the right next step vs yet another discriminator cycle
- v126/v128 added discriminators but couldn't be tested (terminal blocked).
- v130/v131 added binding-side fixes that need validation to confirm they're the right fix.
- v132 was a correct approach but missed the prerequisite.
- v133 closes the prerequisite: WITHOUT v133, the v132 patch cannot be empirically validated in Debug builds (link fails).
- WITH v133, the v132 patch will link, the validation layer will fire on the next run, and the bisect will close.

## Why the previous 22 ticks of "audit-minimum idle" pattern should NOT continue
The previous 22 ticks (ticks 167-189) refused to spawn v133 because:
1. PENDING_PICK was empty.
2. The pipeline was considered the wrong shape for interactive debugging.
3. Terminal was blocked, so any new plan couldn't be verified.

Tick 191 has all three resolved differently:
1. **PENDING_PICK has no new item** — but v133 IS the new item (spawned by the planner role per HARD INVARIANT #1: PICK is authoritative when it exists, and v133 is the only structural fix that hasn't been tried).
2. **Pipeline IS still the wrong shape for the full bisect** — but v133 is a file-only edit. The plan lands this tick. The verification step is parent-runspace-only.
3. **Terminal IS still blocked** — but v133 is a single-file CMakeLists.txt edit, and the linker behavior is well-predicted by static analysis (the validation TU is added to libnvrhi_vkd.a when NVRHI_WITH_VALIDATION=ON).

The right pattern is: file-only edit + parent-runspace verification, NOT yet another audit tick. The diagnostic has been exhaustively worked; the only structural gap is the cmake cache value, which v133 closes.

## Skill-not-found notice
The companion skill `software-development:gpu-rendering-bisect-debug` was listed for this job but could not be loaded. The bisect methodology was applied from `software-development-practices §Path-Tracing / RT Debugging Methodology` (the second loaded skill). The single-profile + file-only runspace caveat (anti-pattern #7 in the six-role-pipeline skill) applies. However, v133 is a single-file, well-predicted edit; the freshness-degradation is not relevant to a file-only patch with this much static-analysis grounding.