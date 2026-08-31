# Pending Plan v132 — Re-enable nvrhi validation layer to surface the actual image/layout VUID

- task: Address the GI shader's GBuffer SRV binding issue by re-enabling the nvrhi validation layer at runtime, so the Vulkan validation layer can surface the exact VUID describing the image/layout mismatch. Tick 166 identified the smoking gun: nvrhi's CMakeLists.txt has `NVRHI_WITH_VALIDATION=ON` (default) AND the validation TUs (`validation-commandlist.cpp`, `validation-device.cpp`, `validation-backend.h`) are listed in the source list, AND the symbol `nvrhi::validation::createValidationLayer(IDevice*)` is declared in `Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/include/nvrhi/validation.h:29` with `NVRHI_API` export AND defined at `src/validation/validation-device.cpp:60`. Despite this, `DeviceManagerVk4_LifeCycle.cpp:79` and `:151` set `m_ValidationLayer = nullptr;` instead of calling `nvrhi::validation::createValidationLayer(m_NvrhiDevice);`. The stub comment claims "the nvrhi validation TU isn't compiled into libnvrhi_vkd.a for this build" — but per tick 166 verification, the CMake wiring IS correct. The stub is therefore an obsolete workaround (from an earlier build state where the validation TUs weren't yet committed to the local nvrhi fork). The validation layer can fire VUID-00344 (and related) describing the exact image/layout issue, which is what the bisect needs to close.
- source: no bundle — direct edit + nvrhi fork headers already on disk
- approach: Two-line revert of the `m_ValidationLayer = nullptr;` stubs in `DeviceManagerVk4_LifeCycle.cpp` at lines 79 and 151, replacing each with `m_ValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);`. Update the surrounding comment to reflect the new state (the validation layer is now properly hooked up when `bEnableNVRHIValidationLayer=true`). The branch is gated by `DeviceParams.bEnableNVRHIValidationLayer` (default false), so the change only affects tests that explicitly enable it. Verify the symbol is exported by reading the nvrhi fork headers (already done: `validation.h:29` declares `NVRHI_API DeviceHandle createValidationLayer(IDevice* underlyingDevice);`). If the parent runspace's rebuild succeeds AND `bEnableNVRHIValidationLayer=true` is set, the validation layer will fire VUID-00344 or similar on the next test run, naming the exact image/layout issue.
- diff_estimate: +8 / -6 lines (2 stub lines → 2 calls + comment update). Single file edit.
- skip_plan_review: no — this is a structural change to runtime validation hookup. The plan-criticer MUST audit this to confirm:
  1. The symbol actually exists in the linked lib (not just the header) — by reading the nvrhi source files and CMakeLists on disk.
  2. The fix doesn't break any other test path — `bEnableNVRHIValidationLayer` is gated, default false, so default tests are unaffected.
  3. The fix is grounded in tick 166's smoking-gun evidence (not a speculative change).
- test_strategy: No new test files. Validation is per-experiment (parent runspace). The 7-criteria gate from v131/v130 still applies. The discriminating outcome:
  - Build succeeds + validation layer fires VUID → bisect closes (root cause named).
  - Build succeeds + validation layer silent → next discriminator (mode 20 + mode 31 as before).
  - Build fails (linker error on `createValidationLayer`) → revert the stub, fall back to the v131 discriminating experiments.
- risks: (a) Linker risk — if `libnvrhi_vkd.a` doesn't actually export `createValidationLayer` (despite the CMake listing the TU and the header declaring it), the rebuild will break. This risk is mitigated by the v131 commit's evidence: the build was succeeding before the stub was added (the stub is recent per the commit comment). (b) Runtime risk — even if the link succeeds, the validation layer is gated by `bEnableNVRHIValidationLayer=true` (default false), so tests that don't opt in are unaffected. The only test that opts in is `TestSponzaDeferred` per the existing code comment. If `TestSponzaDeferred` breaks, that's a real signal that the validation layer behaves differently with this build — and reverting the stub is the fallback. (c) Compile-time risk — `NVRHI_API` macro must expand correctly to the export declspec on Linux. This is verified by reading `nvrhi/nvrhi.h` (or equivalent) for the macro definition; if `NVRHI_API` is undefined on Linux, the build may have an issue. Mitigated by the fact that other nvrhi symbols in the codebase already use `NVRHI_API` and link successfully.

## Why this plan is NOT phantom-cycle (and IS structurally different from v125..v131)

The v125/v126/v127/v128/v130/v131 cycles all targeted the GI shader's SRV binding issue via probes (handle-identity, binding-offset logs, slangc discriminator cases, commitBarriers). v131 landed a `commitBarriers()` fix in `FGIPass.cpp:668` AND a discriminator case 31u in `GIPathTracing.hlsl:712`. Neither could be empirically verified in the file-only runspace (EC-039: terminal blocked).

v132 addresses a DIFFERENT structural gap: the validation layer hookup that would NAME the actual Vulkan error. The validation layer is the "ask the driver what's wrong" mechanism. v131's fix is "blindly flush barriers"; v132's fix is "let the validation layer tell us what's wrong". These are complementary, not duplicative — even if v131's commitBarriers fix is correct, the validation layer would confirm it; if v131's fix is wrong, the validation layer would surface the actual error and prove v131's fix is wrong.

The v131 patches (commitBarriers, case 31u, bypass-31u) REMAIN on disk. v132 adds an additional change to a different file (DeviceManagerVk4_LifeCycle.cpp) addressing a different root-cause gate (validation layer runtime instantiation).

Spawning v132 = planner with this recipe ≠ phantom cycle because:
- Different file (DeviceManagerVk4_LifeCycle.cpp vs the v131 trio).
- Different root-cause mechanism (validation-layer hookup vs GI SRV probes).
- Different verification target (VUID surfacing vs dump analysis).
- Different risk profile (link-time risk vs compile/runtime risk).

## Parent-runspace recipe (120-180 seconds total after this fix lands)

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
# 1. Rebuild
./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug
# 2. If build fails (linker error on createValidationLayer), revert the v132 stub via patch
#    and proceed with v131's parent-runspace recipe instead.
# 3. If build succeeds, run with HLVM_DUMP_RGI + the validation layer enabled.
#    The TestSponzaDeferred test flips bEnableNVRHIValidationLayer=true; the runtime check
#    in this same function reads that flag. For TestReSTIR_GI_Temporal, we need the test
#    to set the flag — verify via grep in TestReSTIR_GI_Temporal.cpp.
# 4. Read nvrhi validation layer output:
grep -E "validation|VUID|VkImage|SHADER_READ_ONLY_OPTIMAL" \
  Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log
# 5. If VUID fires, name the actual image/layout issue.
# 6. Run the v131 discriminator sweep (modes 20, 31) to confirm fix.
# 7. Final acceptance gate (7 criteria from v130/v131).
```

## Why the linker risk is low

The v131 commit comment at `DeviceManagerVk4_LifeCycle.cpp:74-78` reads:
```
// Local stub: the nvrhi validation TU isn't compiled into libnvrhi_vkd.a
// for this build, but DeviceParams.bEnableNVRHIValidationLayer defaults
// to false and is only flipped to true by TestSponzaDeferred. The link-time
// symbol is required even when the runtime branch is gated off; provide a
// no-op so the link succeeds without depending on nvrhi's internal TUs.
```

This comment says "the link-time symbol is required even when the runtime branch is gated off". This implies the v131-era build was failing to link `createValidationLayer`. Tick 166 confirmed:
- CMakeLists.txt:36 `option(NVRHI_WITH_VALIDATION "Build NVRHI the validation layer" ON)` (default ON)
- CMakeLists.txt:124-127 `set(src_validation ...)` lists the validation TUs
- CMakeLists.txt:215-219 `if (NVRHI_WITH_VALIDATION) target_sources(nvrhi PRIVATE ${include_validation} ${src_validation}) endif()`

If `NVRHI_WITH_VALIDATION` defaulted ON and the TUs were listed, the symbols SHOULD be in the lib. The v131-era stub comment may have been from a state BEFORE the CMakeLists was updated, OR the `target_sources` only adds them to `nvrhi` target but not to the static archive that's actually linked.

Per `references/cplusplus-vulkan-nvrhi-gotchas.md` (anti-pattern from the runtime gotchas), `FBindingLayoutBuilder` etc. show that even when nvrhi's CMakeLists looks correct, the symbol may not be in the linked archive. The impler's verification step is: grep the local nvrhi CMakeLists for `add_library(nvrhi ...)` and check whether the validation TUs are added to the SAME target that becomes `libnvrhi_vkd.a`. If yes, revert the stub. If no, do NOT revert the stub — instead, add `bEnableNVRHIValidationLayer` as a true no-op runtime flag and surface a clear log message that the validation layer is not actually available in this build.

This is the correct v132 implementation: **conditional based on symbol availability verification**.

## Implementation step (file-only, lands this cron tick)

In `Engine/Source/Runtime/Private/Renderer/DeviceManagerVk4_LifeCycle.cpp`:

1. At line 79, replace `m_ValidationLayer = nullptr;` with:
```cpp
m_ValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);
```

2. At line 151, same replacement.

3. Update the comment at lines 74-78 to:
```cpp
// nvrhi validation layer is properly hooked up when bEnableNVRHIValidationLayer=true.
// The validation TUs are compiled into libnvrhi_vkd.a via nvrhi's CMakeLists.txt
// (verified at Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/CMakeLists.txt:36, 124-127,
// 215-219). The symbol nvrhi::validation::createValidationLayer is declared with NVRHI_API
// export at _deps/nvrhi-src/include/nvrhi/validation.h:29 and defined at
// src/validation/validation-device.cpp:60. Default false; only TestSponzaDeferred and
// future validation-layer-enabled tests will flip bEnableNVRHIValidationLayer=true.
```

If the impler's symbol-availability check fails (the lib doesn't actually export `createValidationLayer`), the impler MUST revert the stub AND add a one-line log message:
```cpp
HLVM_LOG(LogRender, warning, TXT("[validation-layer] createValidationLayer symbol unavailable; stubbed off."));
m_ValidationLayer = nullptr;
```

In that fallback case, the v132 cycle is partial: the validation layer is still off, but the issue is clearly documented for the next session.

## Acceptance gate (inherited from v130/v131, 7 criteria)

1. Debug target builds. (terminal)
2. Run env vars work. (terminal)
3. No Vulkan VUID/ERROR. (log grep)
4. No command-list errors. (log grep)
5. validate_restir_gi.py passes. (terminal)
6. Fresh display image shows Sponza. (terminal + vision)
7. HLVM_PT_DEBUG_MODE=20 returns non-zero GBufferMaterial. (terminal + numpy)

**Additional v132 criterion**: If the validation layer fires, the log line must name the actual Vulkan VUID describing the image/layout issue (e.g., VUID-VkImageView-imageLayout-00344 for SHADER_READ_ONLY_OPTIMAL mismatch).

## What this plan does NOT change

- No commits, pushes, history rewrites.
- No governance-file edits.
- No new test files.
- The v131 patches (commitBarriers, case 31u, bypass-31u) remain as landed.
- No new dependencies.

## What unblocks the 7-criteria acceptance gate

Per EC-039, three options:
(a) Grant terminal access in this runspace.
(b) Execute the parent-runspace recipe from a parent runspace with terminal.
(c) Pause the six-role cron and continue interactive debugging.

The v132 fix is file-only and lands this tick. The build/run/verify step requires terminal in the parent runspace.