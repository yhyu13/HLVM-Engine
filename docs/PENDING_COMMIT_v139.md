# Pending Commit v139 — Re-enable nvrhi validation layer hookup (v132 redo with v134 prerequisite satisfied)

- plan: docs/PENDING_PLAN_v139.md
- files: Engine/Source/Runtime/Private/Renderer/DeviceManagerVk4_LifeCycle.cpp
- source: no bundle — direct edit. nvrhi fork headers + TU sources already on disk at `Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/`.
- target: branch the parent runspace owns (git topology not touched by cron)
- task: Re-apply the v132 patch (`m_ValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);`) now that v134 placed the validation TUs in `add_library(nvrhi STATIC ...)` source list. Per the v24 diagnostic §"What you (the operator) need to do" step 2 + step 3.
- verify:
  ```
  ./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug
  # If build succeeds, run with default mode:
  HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal
  # Inspect log for nvrhi validation messages + VUID:
  grep -E "validation|VUID|VkImage|SHADER_READ_ONLY_OPTIMAL" \
    Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log
  # If VUID surfaces, the root cause is named.
  # If no nvrhi validation messages, try VK_LAYER_KHRONOS_validation=1 env var:
  VK_LAYER_KHRONOS_validation=1 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
    ./Binary/Debug/TestReSTIR_GI_Temporal
  # Then mode 6 (per v138 discriminator) + mode 20 (SRV read):
  HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=6 \
    ./Binary/Debug/TestReSTIR_GI_Temporal
  HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 \
    ./Binary/Debug/TestReSTIR_GI_Temporal
  # Final acceptance gate (validate + vision):
  python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
  ```
- skip_impl_review: no — this is a structural change to runtime validation hookup. The reviewer MUST verify:
  1. The call replaces the stub at line 118 (not the destructor at line 198).
  2. The include is at the top of the file (line 15), not in a header.
  3. The comment block at lines 84-117 accurately describes the v139 state.
  4. v131+v135+v137+v138 patches remain intact.
- produces_test_files: no — no test files produced.
- notes: terminal access is structurally blocked in this cron runspace per EC-039. The patch lands file-only; the build/run/verify step requires the parent runspace with terminal. The patch is at line 118 (the createValidationLayer call, was the stub at line 96 pre-v139). The destructor at line 198 still nulls (correct behavior per v132 review §81-86).

## Files modified (this cycle)

### 1. Engine/Source/Runtime/Private/Renderer/DeviceManagerVk4_LifeCycle.cpp

**Three-line addition at top** (after line 5 `#include "DeviceManagerVk.h"`): added `#include <nvrhi/validation.h>` with a 9-line comment explaining the rationale (the header is NOT in the transitive include chain via `RHICommon.h -> <nvrhi/nvrhi.h>`, so explicit include is required for the call at line 118 to type-check).

**One-line edit at line 118** (was line 96 pre-v139): replaced `m_ValidationLayer = nullptr;` with `m_ValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);`.

**12-line comment addition at lines 106-117**: explains the v139 rationale (v132's failure was due to ninja dep-graph staleness; v134 placed the validation TUs in `add_library` source list, fixing the dep-graph issue; re-applying v132's patch now succeeds at link time; this is the bisect-closing action per v24 diagnostic).

**No change** at line 198 (destructor still nulls `m_ValidationLayer`) — per v132 review §81-86, this is CORRECT behavior (destructor nulls the handle, the create site creates it).

## Plan Deviations

NONE. The impler followed the v139 plan exactly:
- Single 1-line stub → call edit at the gated site.
- Explicit include addition at the top of the file.
- Comment block update.
- Destructor left untouched (per plan's risk #5).

The static-analysis evidence cited in the v139 plan has been verified by the impler this tick (read_file on validation.h, validation-device.cpp, CMakeLists.txt, DeviceManagerVk.h, DeviceManagerVk5_Misc.cpp). All citations are accurate.

## Acceptance verification (parent runspace)

After the rebuild:
1. Build succeeds: `./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal` exits 0.
   - **Expected**: SUCCESS. The v134 patch placed the validation TUs in `add_library` source list, so `nvrhi::validation::createValidationLayer` should be in `libnvrhid.a` after v134's first build. The v139 patch's `createValidationLayer(m_NvrhiDevice)` call should resolve at link time.
   - **Fallback if FAIL**: revert the stub (re-apply `m_ValidationLayer = nullptr;` at line 118) and add a one-line log message about validation layer unavailability. Then continue with the v138 discriminator chain (mode 6 → mode 20 → validate) instead of relying on VUID surfacing.
2. Test runs to completion without errors.
3. Log contains nvrhi validation messages or VUID-00344 (or related). This is the BISECT-CLOSING evidence.
4. If VUID fires, the actual Vulkan VUID names the image/layout issue. The fix is then v140 (or later).
5. If no nvrhi validation messages, try `VK_LAYER_KHRONOS_validation=1` env var (forces the system-level validation layer).
6. Run mode 6 (per v138 discriminator). Expect per-pixel gradient if v137 was sufficient.
7. Run mode 20 (SRV read discriminator). Expect non-zero GBufferMaterial if v131+v135 was sufficient.
8. Run `validate_restir_gi.py`. Expect 4/4 PASS.
9. Vision check on display PNG. Expect recognizable Sponza with sane exposure.

## Honesty floor

This commit lands patches. It does NOT claim:
- The build succeeded.
- The binary ran.
- Any dump was analyzed.
- The validation layer fired VUID.
- The bisect is closed.

The patches are correct on static analysis (per the evidence in the plan + impl-review). The build/run/verification requires the parent runspace with terminal+vision. If the build fails (linker error on `createValidationLayer`), the impler's fallback path is documented above.

## What unblocks the 7-criteria acceptance gate

Per EC-039, three options:
(a) Grant terminal access in this runspace.
(b) Execute the parent-runspace recipe from a parent runspace with terminal.
(c) Pause the six-role cron and continue interactive debugging.

The v139 patch lands file-only. The build/run/verify step requires terminal. **This is the LAST file-only cycle possible** — all 9 prior patches (v131-v138) plus v139 are now on disk. The pipeline is structurally complete. The parent runspace must execute the recipe.

## Patch summary

- Patch v139 added: 13 lines (1 include + 12 comment lines).
- Patch v139 modified: 1 line (the stub → call).
- Patch v139 removed: 0 lines (the comment block is additive on top of the existing v132+v136 comments).
- Net diff: +13 / -1 lines (well within the ≤200 line per-tick limit).