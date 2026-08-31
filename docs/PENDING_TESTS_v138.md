# Pending Tests v138
- task: Add `6u` to `bypassEarlyReturn` debug-mode list in GIPathTracing.hlsl so mode 6 actually discriminates UAV-bug vs SRV-bug
- tester: tester (file-only single-profile mode)
- timestamp: 2026-07-31

## Test file

No new test file produced. v138 is a diagnostic-mode patch, not a behavioral change. The actual behavior tests are deferred to the parent runspace.

## File-only verification (run in this turn, no terminal required)

1. **Patch applied**: `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl:486` now contains:
   ```hlsl
   bool bypassEarlyReturn = (debugModeEarly == 6u
                          || debugModeEarly == 20u
                          || debugModeEarly == 21u
                          || debugModeEarly == 22u
                          || debugModeEarly == 30u
                          || debugModeEarly == 31u);
   ```
   `6u` is the FIRST entry in the `||` chain. Read back via search_files — confirmed.

2. **Comment block intact**: Lines 475-485 contain the v138 reasoning comment explaining why `6u` was added (the re-analysis that mode 6 was masked by the early-return).

3. **No dangling reference**: `search_files` for `bypassEarlyReturn` returns only the 2 expected matches in GIPathTracing.hlsl (definition at line 486, use at line 493).

4. **v131+v135+v136+v137 patches intact**:
   - `FGIPass.cpp:557-562` (v135 commitBarriers) present
   - `FGIPass.cpp:675` (v131 commitBarriers) present
   - `FGIPass.cpp:313-318` (v137 UAV binding-offset) present
   - `DeviceManagerVk4_LifeCycle.cpp:96, 176` have `m_ValidationLayer = nullptr;` (v136 revert)
   - `GIPathTracing.hlsl:685-687, 712-714` have cases 20u/21u/22u/30u/31u discriminator entries

5. **GIPathTracing.hlsl debug modes intact**: cases 1u-15u + 20u-22u + 30u + 31u all present.

## Behavioral tests (terminal+vision required, deferred to parent runspace)

The parent must:

1. Run `./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug` — should succeed (no compile error, slangc picks up the change).

2. **THE DISCRIMINATOR** — Run mode 6 with the full gradient expectations:
   ```bash
   HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=6 ./Binary/Debug/TestReSTIR_GI_Temporal
   ```
   Inspect `dumps/<timestamp>_gi_raw_frame8.png` with numpy per-channel stats.

3. **Decision tree** based on mode 6 dump:
   - **Mode 6 shows per-pixel gradient** (red on x-axis from 0..1, blue on y-axis from 0..1, green=0, alpha=254): v137 fixed the UAV bug. Mode 20/21/22 should ALSO now return non-zero (they were masked by the same early-return at line 493-495). Proceed to step 4.
   - **Mode 6 dump is still all-zero**: v137 was wrong-fix for symptom. SRV bug is present. v139 will investigate (a) image layout transition for first-frame reads, (b) slangc dead-strip (falsified by v24 spirv-dis but worth re-checking), (c) Output UAV mis-bind.

4. **If mode 6 shows gradient** — confirm SRV reads with mode 20:
   ```bash
   HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 ./Binary/Debug/TestReSTIR_GI_Temporal
   ```
   Expect: non-zero per-pixel GBufferMaterial values matching the staging dump of `gbuffer_material_frame8.png` (~45% white, 54% near-white).

5. **If mode 20 returns non-zero** — bisect closes, validate:
   ```bash
   python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
   ```
   Expect: 4/4 checks PASS.

6. **Vision check** on display PNG (terminal+vision required):
   ```bash
   # Use vision_analyze or similar on dumps/<timestamp>_display_frame8.png
   ```
   Expect: recognizable Sponza with sane exposure (not pure black, not all-white).

7. **Default mode (no debug flag)** for actual GI path-trace output:
   ```bash
   HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal
   ```
   Expect: non-zero gi_raw dump showing actual GI contributions.

## Test count
- File-only tests: 5 PASS (this turn)
- Behavioral tests: 0/7 runnable in this runspace (deferred)

---

**Per `six-role-pipeline §Role #5 (tester)`, this is a file-only test report. Behavioral tests deferred to parent runspace.**