# Pending Commit v132 — Re-enable nvrhi validation layer hookup (file-only this tick)

- plan: docs/PENDING_PLAN_v132.md
- files:
  Engine/Source/Runtime/Private/Renderer/DeviceManagerVk4_LifeCycle.cpp
- source: no bundle — direct edit
- target: branch the parent runspace owns (git topology not touched by cron)
- task: Revert the `m_ValidationLayer = nullptr;` stubs in `DeviceManagerVk4_LifeCycle.cpp:79` and `:151` to `m_ValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);`. The static-analysis evidence (verified this tick and per tick 166) confirms the symbol IS in the lib:
  - `_deps/nvrhi-src/CMakeLists.txt:200` — `add_library(nvrhi STATIC ...)`
  - `_deps/nvrhi-src/CMakeLists.txt:215-219` — `target_sources(nvrhi PRIVATE ${include_validation} ${src_validation})` (when `NVRHI_WITH_VALIDATION=ON`, which is the default per CMakeLists.txt:36).
  - `_deps/nvrhi-src/include/nvrhi/validation.h:29` — `NVRHI_API DeviceHandle createValidationLayer(IDevice* underlyingDevice);`
  - `_deps/nvrhi-src/src/validation/validation-device.cpp:60` — definition.
  - `_deps/nvrhi-src/include/nvrhi/nvrhi.h:44-61` — `NVRHI_API` macro is empty for non-shared-library builds, so no declspec is needed; the symbol is exported via the static lib's symbol table naturally.
  - `_deps/nvrhi-src/CMakeLists.txt:357-361` — install target is `nvrhi_vulkan_target` (lib name with `_vkd` suffix), confirming `nvrhi` IS the target name that produces `libnvrhi_vkd.a`.
  - `Engine/Source/Runtime/Binary/Debug/libnvrhi_vkd.a` — EXISTS on disk per `.ninja_log` (rebuilt recently).
  - `Engine/Source/Runtime/CMakeLists.txt:174-177` — `FetchContent_Declare(nvrhi ... GIT_REPOSITORY https://github.com/yhyu13/NVRHI.git GIT_TAG 472f99ac68251970dc9e75afa1648c9bc4db7e83)` — confirms nvrhi is fetched from the local fork and the CMakeLists.txt reflects upstream.

- verify:
  ```
  ./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug
  # If build succeeds, run with the validation layer enabled:
  HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal
  # Inspect log for VUID:
  grep -E "VUID|validation|SHADER_READ_ONLY_OPTIMAL" \
    Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log
  # If VUID surfaces, the root cause is named.
  ```

- skip_impl_review: no — this commit modifies runtime device-manager code AND changes a stub that was added specifically to fix a linker error. The reviewer MUST verify:
  1. The symbol is actually exported by `libnvrhi_vkd.a` (not just declared in headers).
  2. The reverted code doesn't break any existing test.
  3. The gating by `bEnableNVRHIValidationLayer` is preserved.

- produces_test_files: no — no test files produced.

- notes: terminal access is structurally blocked in this cron runspace per EC-039. The patch lands file-only; the build/run/verify step requires the parent runspace with terminal.

## Files modified (this cycle)

### 1. Engine/Source/Runtime/Private/Renderer/DeviceManagerVk4_LifeCycle.cpp

**Two-line edit at lines 79 and 151**: replace `m_ValidationLayer = nullptr;` with
`m_ValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);`.

**Comment update at lines 74-78**: replace the obsolete "validation TU isn't compiled"
rationale with the verified CMake wiring + symbol declaration evidence.

The change is gated by `if (DeviceParams.bEnableNVRHIValidationLayer)` — the surrounding
block is unchanged, so the call only happens when `bEnableNVRHIValidationLayer=true`
(default false). Only `TestSponzaDeferred` and any future validation-enabled test
will exercise the new code path.

## Plan Deviations

NONE. The impler followed the v132 plan exactly:
- Two-line revert at lines 79 and 151.
- Comment update at lines 74-78.
- No fallback path needed (the symbol IS in the lib per the static-analysis evidence).

The impler's static-analysis verification (per plan-criticer feedback) was performed
this tick and confirmed:
- Symbol `nvrhi::validation::createValidationLayer` declared in `_deps/nvrhi-src/include/nvrhi/validation.h:29` with `NVRHI_API` (empty for static builds).
- Definition at `_deps/nvrhi-src/src/validation/validation-device.cpp:60`.
- Validation TUs added to the `nvrhi` target via `target_sources(nvrhi PRIVATE ${src_validation})` at `_deps/nvrhi-src/CMakeLists.txt:215-219` (gated by `NVRHI_WITH_VALIDATION=ON`, which is the default per CMakeLists.txt:36).
- `libnvrhi_vkd.a` exists on disk at `Engine/Source/Runtime/Binary/Debug/libnvrhi_vkd.a` and was recently rebuilt (per `.ninja_log`).
- `NVRHI_API` macro expansion on Linux static builds is empty (`_deps/nvrhi-src/include/nvrhi/nvrhi.h:60`), which is fine — the symbol is still exported via the static lib's symbol table.

The verification was performed via `search_files` content+context grep and the
manifestation of evidence is in this commit file. Terminal not required for the
verification itself, only for the build/run step.

## Acceptance verification (parent runspace)

After the rebuild:
1. Build succeeds: `./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal` exits 0.
2. TestSponzaDeferred still builds and runs without errors (regression check).
3. For TestReSTIR_GI_Temporal, the validation layer is NOT enabled by default (gated by `bEnableNVRHIValidationLayer=false`); to exercise it, the parent runspace needs to either (a) flip the flag in TestReSTIR_GI_Temporal.cpp temporarily, or (b) flip the CVar default in the device creation path. The plan-criticer feedback #2 (gating preservation) was honored — the gating is preserved, so the validation layer is silently off for the ReSTIR test until explicitly enabled.
4. If validation layer is enabled AND VUID fires, the log will name the actual image/layout issue. This is the bisect-closing evidence.
5. Run the v131 discriminator sweep (`HLVM_PT_DEBUG_MODE=20`, `HLVM_PT_DEBUG_MODE=31`) to confirm fix.
6. Final acceptance gate (7 criteria from v130/v131).

## Honesty floor

This commit lands patches. It does NOT claim:
- The build succeeded.
- The binary ran.
- Any dump was analyzed.
- The validation layer fired VUID.

The patches are correct on static analysis (per the evidence above). The build/run
verification requires the parent runspace. If the build fails (linker error on
`createValidationLayer`), the impler's fallback path (per the v132 plan) was to
revert the stub and add a one-line log message — but the static analysis makes
this fallback unlikely to be needed.

## What unblocks the 7-criteria acceptance gate

Per EC-039, three options:
(a) Grant terminal access in this runspace.
(b) Execute the parent-runspace recipe from a parent runspace with terminal.
(c) Pause the six-role cron and continue interactive debugging.

The v132 patch lands file-only. The build/run/verify step requires terminal.