# Pending Plan Review v139 — Re-enable nvrhi validation layer hookup

- plan: docs/PENDING_PLAN_v139.md
- verdict: KEEP
- reviewer: plan-criticer (file-only single-profile mode, tick 263)
- timestamp: 2026-07-31

## Design soundness

The v139 plan is the v132 plan re-applied with one structural improvement (explicit `<nvrhi/validation.h>` include) and with the v134 prerequisite now satisfied. v132 was KEEP-verdict at tick 167; v139's design is the same intent, plus an explicit include to make the symbol's declaration visible at compile time. The chain of evidence is solid:

- v24 diagnostic identifies the validation layer hookup as the bisect-closing action.
- v132 attempted this and failed at LINK time because the validation TUs were not yet in `add_library` source list.
- v133+v134 (tick 193) placed the validation TUs in `add_library(nvrhi STATIC ${include_validation} ${src_validation})` at lines 209-214 of `_deps/nvrhi-src/CMakeLists.txt`, with a 12-line comment explaining the durability of this fix against ninja dep-graph regeneration (lines 200-208).
- v139 re-applies the v132 patch now that the link prerequisite is met.

The plan correctly identifies the file-only vs terminal-only split: the patch lands file-only, the build/run/VUID-inspection step is the parent's responsibility per EC-039 (terminal blocked by tirith in this runspace).

## Plan completeness

The plan covers all required elements:
- task, source, approach, diff_estimate, skip_plan_review=no, test_strategy, risks (6 numbered risks).
- Concrete parent-runspace recipe (8 steps).
- Honest about what v139 does NOT solve (the actual image/layout fix once VUID surfaces — that's v140+ work).
- Honest about the validation-layer-enabled-by-default behavioral change (post-v139, `g_UseValidationLayers = true` CVar means `m_ValidationLayer` is no longer always nullptr; this IS the intended activation).

The plan's include addition (`#include <nvrhi/validation.h>`) is a sound improvement over v132. Verified via search_files that `<nvrhi/validation.h>` is NOT in the transitive include chain (`<nvrhi/nvrhi.h>` does not include it; `RHICommon.h` only includes `<nvrhi/nvrhi.h>`). The v132 patch's omission of this include is suspicious — either the compile error was not surfaced in the rebuild log, or some path I haven't traced brings the header in. v139's explicit include is the safe move.

## Feasibility check

Verified all cited file:line references this turn:

| Plan citation | Verified location | Status |
|---|---|---|
| `DeviceManagerVk4_LifeCycle.cpp:96` (stub to replace) | `<m_ValidationLayer = nullptr;>` at line 96, inside `if (DeviceParams.bEnableNVRHIValidationLayer) { ... }` block at lines 72-97 | ✅ CONFIRMED |
| `_deps/nvrhi-src/include/nvrhi/validation.h:29` | `NVRHI_API DeviceHandle createValidationLayer(IDevice* underlyingDevice);` | ✅ CONFIRMED |
| `_deps/nvrhi-src/src/validation/validation-device.cpp:60` | `DeviceHandle createValidationLayer(IDevice* underlyingDevice) { ... }` | ✅ CONFIRMED |
| `_deps/nvrhi-src/CMakeLists.txt:209-214` (v134 patch) | `add_library(nvrhi STATIC ${include_common} ${src_common} ${misc_common} ${include_validation} ${src_validation})` | ✅ CONFIRMED |
| `_deps/nvrhi-src/CMakeLists.txt:36` (NVRHI_WITH_VALIDATION default ON) | `option(NVRHI_WITH_VALIDATION "Build NVRHI the validation layer" ON)` | ✅ CONFIRMED |
| `Engine/Source/Runtime/CMakeLists.txt:182` (v133 force) | `set(NVRHI_WITH_VALIDATION ON CACHE BOOL "Build NVRHI the validation layer" FORCE)` | ✅ CONFIRMED |
| `Engine/Source/Runtime/rebuild_Debug.log:151` (v132 link error) | `undefined reference to 'nvrhi::validation::createValidationLayer(nvrhi::IDevice*)'` | ✅ CONFIRMED |
| `DeviceManagerVk.h:22` (`g_UseValidationLayers = true`) | `HLVM_INLINE_VAR bool g_UseValidationLayers = true;` | ✅ CONFIRMED |
| `DeviceManagerVk4_LifeCycle.cpp:15` (OR into bEnableNVRHIValidationLayer) | `DeviceParams.bEnableNVRHIValidationLayer \|= g_UseValidationLayers;` | ✅ CONFIRMED |
| `DeviceManagerVk4_LifeCycle.cpp:176` (destructor still nulls) | `m_ValidationLayer = nullptr;` in destructor | ✅ CONFIRMED (v139 does NOT change this; per v132 review §81-86, nulling in destructor is CORRECT) |
| `DeviceManagerVk5_Misc.cpp:34-38` (GetDevice uses m_ValidationLayer if non-null) | `if (m_ValidationLayer) { return m_ValidationLayer; } return m_NvrhiDevice;` | ✅ CONFIRMED (post-v139, GetDevice returns the validation-layer-wrapped device when validation is enabled) |
| `nvrhi-src/doc/Tutorial.md:60-65` (canonical usage pattern) | `#include <nvrhi/validation.h> ... nvrhi::DeviceHandle nvrhiValidationLayer = nvrhi::validation::createValidationLayer(nvrhiDevice);` | ✅ CONFIRMED |

All cited references are real and accurate. The plan is feasible to execute.

## Risk re-assessment

The plan's 6 risks are honest and well-grounded. Two are worth re-emphasizing:

- **Risk #1 (link-time risk)** is real but mitigated by v134. If v139 fails to link, the fallback (log message + leave stub) is documented per the v132 plan's fallback path.
- **Risk #3 (validation layer enabled by default)** is the actual BEHAVIORAL CHANGE that makes v139 different from "another no-op patch". The validation layer being enabled by default is the intended behavior change — it's the whole point. Any Vulkan validation errors that surface are the bisect evidence.

## Compared to v132

This is essentially a v132 redo. The differences:
1. Explicit `<nvrhi/validation.h>` include added (improvement).
2. v134 prerequisite now satisfied (was not at v132 time).
3. Risk analysis includes the validation-layer-enabled-by-default behavioral change (v132 plan did not flag this).
4. Parent-runspace recipe is more detailed (8 steps vs 7).

The plan is structurally better than v132 was, and the same intent. KEEP.

## Verdict

**KEEP.** The plan is correctly designed, all cited references are accurate, the feasibility check passes, the risks are honestly acknowledged, and the file-only / terminal-only split is correctly drawn. The dispatcher should route to Agent #3 (impler) on the next tick.

---

**Per `six-role-pipeline §Role #2 (plan-criticer)`, this is a file-only verdict. The impler will execute the plan; the parent runspace will run the build/verify step.**