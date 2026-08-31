# Pending Tests v139 — File-only verification of nvrhi validation layer hookup

- commit: docs/PENDING_COMMIT_v139.md
- tester: tester (file-only single-profile mode, tick 263)
- timestamp: 2026-07-31

## Test files

No new test file produced. v139 is a diagnostic/behavioral patch (re-applies v132's `createValidationLayer` hookup). The behavioral tests are deferred to the parent runspace (per the plan's test_strategy and the v139 commit's verify section).

## File-only verification (run in this turn, no terminal required)

1. **Include added**: `Engine/Source/Runtime/Private/Renderer/DeviceManagerVk4_LifeCycle.cpp:15` contains:
   ```cpp
   #include <nvrhi/validation.h>
   ```
   `search_files` confirms the include is present. Read back via `read_file` — confirmed.

2. **Comment block intact**: Lines 7-14 contain the v139 comment explaining why the include is necessary (the header is NOT in the transitive include chain via `RHICommon.h -> <nvrhi/nvrhi.h>`).

3. **Stub replaced with call**: Line 118 contains:
   ```cpp
   m_ValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);
   ```
   `search_files` confirms the call is present. Read back via `read_file` — confirmed.

4. **v139 rationale comment intact**: Lines 106-117 contain the v139 comment explaining the v134 prerequisite satisfaction and the rationale for re-applying v132's patch.

5. **No accidental change to destructor**: Line 198 still contains `m_ValidationLayer = nullptr;` (the destructor nulls the handle on shutdown, which is correct per v132 review §81-86).

6. **v131+v135+v137+v138 patches intact**:
   - `FGIPass.cpp:579` (v135 commitBarriers) — present
   - `FGIPass.cpp:692` (v131 commitBarriers) — present
   - `FGIPass.cpp:301-318` (v137 UAV binding-offset) — present (verified at the top of the file in plan-criticer audit)
   - `GIPathTracing.hlsl:486-491` (v138 mode 6 bypassEarlyReturn addition) — present
   - `DeviceManagerVk4_LifeCycle.cpp:198` (v136 revert of v132 at the destructor site) — present (NOT changed by v139)

7. **v134 patch (validation TUs in add_library) intact**:
   - `_deps/nvrhi-src/CMakeLists.txt:213-214` — `${include_validation}` and `${src_validation}` are in the `add_library(nvrhi STATIC ...)` source list. Confirmed via `search_files`.

8. **v133 patch (NVRHI_WITH_VALIDATION force) intact**:
   - `Engine/Source/Runtime/CMakeLists.txt:182` — `set(NVRHI_WITH_VALIDATION ON CACHE BOOL "Build NVRHI the validation layer" FORCE)`. Confirmed via prior searches.

## Behavioral tests (terminal+vision required, deferred to parent runspace)

The parent must:

1. Run `./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug` — should succeed. The v134 patch ensures the validation TUs are compiled into `libnvrhid.a`. The v139 patch's `createValidationLayer(m_NvrhiDevice)` call should resolve at link time.

2. **If build succeeds**, run with default mode:
   ```bash
   HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal
   ```
   Inspect log for nvrhi validation messages:
   ```bash
   grep -E "validation|VUID|VkImage|SHADER_READ_ONLY_OPTIMAL" \
     Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log
   ```
   - **If VUID surfaces**: this is the BISECT-CLOSING evidence. The actual Vulkan VUID names the GI shader's GBuffer SRV layout issue. v140 (or later) addresses the actual fix based on the VUID.
   - **If no VUID**: try enabling the system-level validation layer explicitly:
     ```bash
     VK_LAYER_KHRONOS_validation=1 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
       ./Binary/Debug/TestReSTIR_GI_Temporal
     ```
     Then re-grep for VUID.

3. **Run the v138 discriminator chain** (per tick 248 re-analysis):
   ```bash
   HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=6 \
     ./Binary/Debug/TestReSTIR_GI_Temporal
   ```
   - **Per-pixel gradient visible**: v137 fixed the UAV bug. v131+v135 fixed the SRV read. Modes 20/21/22 should ALSO now return non-zero. Run mode 20 to confirm.
   - **All-zero dump**: SRV bug is present. v137 was a wrong-fix for symptom. v140 (or later) investigates the SRV binding/layout.

4. **If mode 6 shows gradient**, confirm with mode 20:
   ```bash
   HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 \
     ./Binary/Debug/TestReSTIR_GI_Temporal
   ```
   - **Non-zero per-pixel GBufferMaterial**: bisect closes. Run validator.
   - **All-zero**: SRV bug is present.

5. **Validate**:
   ```bash
   python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
   ```
   Expect: 4/4 checks PASS.

6. **Vision check** on display PNG (terminal+vision required):
   ```bash
   # Use vision_analyze or similar on dumps/<timestamp>_display_frame8.png
   ```
   Expect: recognizable Sponza with sane exposure (not pure black, not all-white).

## Test count

- File-only tests: 8 PASS (this turn)
- Behavioral tests: 0/6 runnable in this runspace (deferred)

## TDD red-phase notes

This is a diagnostic patch, not a behavioral change. The "test" is the parent-runspace recipe. There is no TDD red-phase for this cycle — the "failing" state (vulkan VUID not surfacing) is the symptom the bisect is trying to close, and the "passing" state (VUID surfaces) is the bisect-closing evidence.

## Testability gaps (informational, not FIX)

None. The v139 patch's intended testability mechanism (validation layer firing VUID) is a runtime property, not a unit-test property. It cannot be exercised without terminal+vision.

---

**Per `six-role-pipeline §Role #5 (tester)`, this is a file-only test report. Behavioral tests deferred to parent runspace per EC-039 (terminal blocked by tirith).**