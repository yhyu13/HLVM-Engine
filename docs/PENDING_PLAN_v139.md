# Pending Plan v139 — Re-enable nvrhi validation layer hookup (v132 redo with v134 prerequisite satisfied)

- task: Re-apply the v132 patch (`m_ValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);`) now that v133+v134 have placed the validation TUs in `add_library(nvrhi STATIC ...)` source list, which v24's "What you (the operator) need to do" identified as step 1 of the 4-step recipe. v132 was attempted at tick 167 (before v133+v134) and failed at LINK time because ninja dep-graph staleness skipped the validation .o files. v134 explicitly addresses this dep-graph issue by placing the TUs in `add_library` source list (lines 209-214 of `Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/CMakeLists.txt`). The build now should link `createValidationLayer` successfully.
- source: no bundle — direct edit. nvrhi fork headers + TU sources already on disk at `Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/`.
- approach: Single 1-line edit at `Engine/Source/Runtime/Private/Renderer/DeviceManagerVk4_LifeCycle.cpp:96` (currently `m_ValidationLayer = nullptr;`), replacing with `m_ValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);`. Update the v136 comment block at lines 85-95 to reflect the new state (v132 prerequisite satisfied by v133+v134; v134 explicitly re-lists the validation TUs in `add_library` so the link should now succeed). ALSO add `#include <nvrhi/validation.h>` at the top of `DeviceManagerVk4_LifeCycle.cpp` (after line 5 `#include "DeviceManagerVk.h"`) to make the symbol's declaration visible at compile time. The current `RHICommon.h` include chain brings in `<nvrhi/nvrhi.h>` but NOT `<nvrhi/validation.h>` — explicit include is the safe move. The branch is gated by `if (DeviceParams.bEnableNVRHIValidationLayer)` (default false via `g_UseValidationLayers = true` CVar ORed in), so only tests that opt in via `bEnableNVRHIValidationLayer=true` will exercise the new code path. The test's `Params.bEnableDebugRuntime = true;` at TestReSTIR_GI_Temporal.cpp:1905 is independent — `bEnableNVRHIValidationLayer` is controlled by the CVar default `g_UseValidationLayers = true` which ORs into `DeviceParams.bEnableNVRHIValidationLayer` at DeviceManagerVk4_LifeCycle.cpp:15, so by default the validation layer IS enabled in the test (the OR is the activation mechanism).
- diff_estimate: +5 / -3 lines (1 stub line → 1 call + include + comment update). Single file edit.
- skip_plan_review: no — this is the SAME structural change as v132 (which was KEEP-verdict). The plan-criticer should re-audit because (a) the v132 failure was real and the v134 fix needs verification, (b) the include-question (does adding `<nvrhi/validation.h>` break anything?) is novel for v139, (c) the test gating question (does `g_UseValidationLayers = true` actually mean the validation layer is now enabled in TestReSTIR_GI_Temporal by default?) is novel for v139.
- test_strategy: role #5 (tester) verifies file-only state: (a) `DeviceManagerVk4_LifeCycle.cpp:96` contains the new call (not the old stub), (b) `<nvrhi/validation.h>` is included, (c) comment block at lines 85-95 is updated, (d) v131+v135+v137+v138 patches intact, (e) the v134 patch (validation TUs in `add_library` source list at `Build/Debug/_deps/nvrhi-src/CMakeLists.txt:209-214`) is intact. No behavioral test — that requires terminal+vision per EC-039.
- risks:
  1. **Link-time risk (the v132 failure mode)** — the v132 patch's link error was `undefined reference to 'nvrhi::validation::createValidationLayer(nvrhi::IDevice*)'` per `Engine/Source/Runtime/rebuild_Debug.log:151`. v134 explicitly addresses this by placing the validation TUs in `add_library(nvrhi STATIC ...)` source list (lines 209-214), with a 12-line comment explaining the durability of this fix (lines 200-208). The next rebuild after v139 SHOULD link successfully. If it does NOT, the fallback is to revert the stub and add a one-line log message about validation layer unavailability (per v132 plan §"If the impler's symbol-availability check fails"). Mitigation: the v134 patch is in place; the next rebuild picks it up automatically.
  2. **Compile-time risk (include chain)** — `<nvrhi/validation.h>` is NOT transitively included via `<nvrhi/nvrhi.h>` (verified via search_files). Adding `#include <nvrhi/validation.h>` explicitly to `DeviceManagerVk4_LifeCycle.cpp` is required for the `nvrhi::validation::createValidationLayer` call to type-check. The v132 patch did NOT add this include (per its plan/commit documents), yet the rebuild log shows only a LINK error not a COMPILE error — this is suspicious. Two possibilities: (a) the compile DID fail but was not the focus of the rebuild log shown, or (b) some include path I haven't traced brings in `<nvrhi/validation.h>` transitively. v139 adds the include explicitly to be safe.
  3. **Validation layer enabled by default in TestReSTIR_GI_Temporal** — `g_UseValidationLayers = true` at `DeviceManagerVk.h:22` defaults to true. `DeviceParams.bEnableNVRHIValidationLayer |= g_UseValidationLayers;` at `DeviceManagerVk4_LifeCycle.cpp:15` ORs the CVar in. So by default, the test runs WITH the validation layer enabled. This is a BEHAVIORAL CHANGE — pre-v139, `m_ValidationLayer` was always `nullptr` so the validation layer never actually wrapped the device. Post-v139, the validation layer IS wrapping the device. This MAY surface Vulkan validation layer errors that were previously masked. **Mitigation**: the validation layer's purpose is to surface these errors. Any errors that fire are the BISECT evidence the v24 diagnostic identified as needed. Errors are not regressions — they are signal.
  4. **Validation layer may slow the dispatch** — nvrhi's validation layer adds overhead per-dispatch. TestReSTIR_GI_Temporal has a 30-second safety timer at line 1927 (`const double TimeoutSec = std::max(30.0, 16.0)`). If validation layer overhead pushes the dispatch over this, the test will exit early. Mitigation: the safety timer was 30s but minimum 16s; the dispatch budget is generous. If the timer fires early, the test should still dump gi_raw from the partial accumulation.
  5. **Destructor still nulls (correct behavior)** — `DeviceManagerVk4_LifeCycle.cpp:176` has `m_ValidationLayer = nullptr;` in the destructor. Per v132's impl-review (line 81-86 of PENDING_IMPL_REVIEW_v132.md), this is CORRECT — the destructor nulls the handle, the create site creates it. v139 does NOT change the destructor.
  6. **v139 may not close the bisect** — even with validation layer enabled and VUID surfacing, the bisect may need further work (e.g., enabling `VK_LAYER_KHRONOS_validation=1` env var to get the VUID text, not just the nvrhi validation layer output). v139 is a NECESSARY but possibly not SUFFICIENT step toward closing the bisect. The v24 diagnostic's 4-step recipe identifies all the steps; v139 is the file-only portion of step 2 + step 3 (step 1 was v134, step 4 is parent-runspace).

**Concrete parent-runspace recipe after v139 lands** (terminal+vision required):

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine

# 1. Rebuild (picks up all 9 prior patches + v139 automatically)
./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug
# Expected: SUCCESS (validation TU symbols now in libnvrhid.a via v134 + v139 stub revert)

# 2. Run with default mode (validation layer enabled by CVar default)
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal
# Expected: log lines containing nvrhi validation messages (if any errors) or clean run

# 3. Grep for nvrhi validation output
grep -E "validation|VUID|VkImage|SHADER_READ_ONLY_OPTIMAL" \
  Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log
# Expected: lines naming the actual image/layout issue (or empty if no VUID)

# 4. If no VUID from nvrhi validation layer, try enabling VK_LAYER_KHRONOS_validation=1 env var
VK_LAYER_KHRONOS_validation=1 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
  ./Binary/Debug/TestReSTIR_GI_Temporal
grep -E "VUID|VkImage|SHADER_READ_ONLY_OPTIMAL|VUID-VkImageView-imageLayout-00344" \
  Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log

# 5. Run with mode 6 (per v138 discriminator)
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=6 \
  ./Binary/Debug/TestReSTIR_GI_Temporal
# Inspect dumps/<timestamp>_gi_raw_frame8.png with numpy. If gradient visible, v137 was
# sufficient + v138 discriminator enabled. If still all-zero, v139 didn't help; v140+ investigate.

# 6. Run with mode 20 (GBufferMaterial SRV read discriminator)
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 \
  ./Binary/Debug/TestReSTIR_GI_Temporal
# If mode 20 returns non-zero, the SRV binding bug is fixed by v131+v135 (or v137+v138).

# 7. Run validator
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
# Expect: 4/4 checks PASS if mode 20 returned non-zero.

# 8. Vision check on display PNG
# Expect: recognizable Sponza with sane exposure (not pure black, not all-white).
```

**Why this is the right next step**:
- v24 diagnostic's 4-step recipe explicitly identified the validation layer hookup as the bisect-closing action.
- v133+v134 (tick 193) addressed step 1 (validation TUs in `add_library`).
- v139 addresses steps 2 + 3 (revert stub + enable validation).
- Step 4 (rebuild + run + VUID surface) is the parent's responsibility per EC-039.
- This is the LAST file-only cycle possible. After v139 lands, the pipeline is structurally complete and the parent runspace must execute the recipe.

**Per `software-development-practices §Plan Mode`, this plan is delivered as a markdown file. No file edits performed by this role.**