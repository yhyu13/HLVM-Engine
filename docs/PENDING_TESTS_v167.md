# Pending Tests v167
- commit: docs/PENDING_COMMIT_v167.md
- plan: docs/PENDING_PLAN_v167.md
- reviewer: docs/PENDING_IMPL_REVIEW_v167.md
- role: tester (file-only, single-profile host)
- timestamp: 2026-08-22T00:00:30Z

## Test role scope

The v167 commit reverts the v166 patch (-22 lines from `vulkan-raytracing.cpp:1643-1665`) and adds an explicit dynamic-state-clear (+10 lines to `vulkan-raytracing.cpp::setRayTracingState` before line 1347). Both edits target the nvrhi fork in `_deps/`, which is git-ignored (FetchContent output).

The cron runspace cannot rebuild + run + validate + vision-check because the terminal is blocked by tirith (cumulative ≥955 denials in this profile). The tester's role is **documentation**: list every operator-side verification step, the expected outputs, and the failure modes.

## Operator-side verification recipe (10 steps)

### Step 1: Verify the revert (Part 1) is on disk in Debug copy
```bash
grep -n 'setPDynamicState' Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp
```
**Expected**: 0 hits (v166 patch fully reverted).

**Fail modes**:
- 1+ hits → revert not applied → re-apply Part 1 of `PENDING_COMMIT_v167.md`
- grep error → file path typo

### Step 2: Verify the add (Part 2) is on disk in Debug copy
```bash
grep -n 'v167 (six-role-pipeline' Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp
```
**Expected**: 1 hit at the comment header above `setViewport(0,0,nullptr)` call (line ~1348).

**Fail modes**:
- 0 hits → add not applied → re-apply Part 2 of `PENDING_COMMIT_v167.md`
- 2+ hits → Part 2 applied twice

### Step 3: Verify both edits are in Release and RelWithDebInfo copies
```bash
grep -n 'v167 (six-role-pipeline' Engine/Source/Runtime/Build/Release/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp
grep -n 'v167 (six-role-pipeline' Engine/Source/Runtime/Build/RelWithDebInfo/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp
grep -n 'setPDynamicState' Engine/Source/Runtime/Build/Release/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp
grep -n 'setPDynamicState' Engine/Source/Runtime/Build/RelWithDebInfo/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp
```
**Expected**: 1 hit each for `v167 (six-role-pipeline`, 0 hits each for `setPDynamicState`.

**Fail modes**:
- Any 0 hits on `v167` → Release or RelWithDebInfo copy not patched
- Any 1+ hits on `setPDynamicState` → v166 patch not fully reverted in that copy

### Step 4: Force CMake reconfigure so FetchContent detects the patch
```bash
cd Engine/Source/Runtime/Build/Debug && cmake -S ../../.. -B . -DCMAKE_BUILD_TYPE=Debug && cd -
```
**Expected**: CMake configures successfully. No fatal errors.

**Fail modes**:
- CMake errors → FetchContent failed to refresh; check `Build/Debug/_deps/nvrhi-src/` for the patch mtime with `stat -c '%y' _deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp`
- "Cannot find source file" → nvrhi fork was not re-fetched; check `find_engine_source.nvrhi` step

### Step 5: Rebuild
```bash
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
```
**Expected**: builds successfully. The new `libnvrhi_vkd.a` includes the patched `vulkan-raytracing.cpp` (with v166 reverted AND Part 2 added).

**Fail modes**:
- Compile error in `vulkan-raytracing.cpp` → the patch introduced a syntax error; check that `setViewport(0, 0, nullptr)` and `setScissor(0, 0, nullptr)` calls match `vk::CommandBuffer::setViewport(uint32_t, uint32_t, const vk::Viewport*)` signature
- Link error → the patch broke a symbol; check `engine_nvrhi` target depends on the patched lib

### Step 6: Run with mode-20 discriminator + RGI dumps
```bash
HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
  ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal
```
**Expected**: the binary runs for 8 frames, dumps `20260822_*gi_raw_frame*.png` into `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/`, and produces a new `Binary/Debug/TestReSTIR_GI_Temporal.log` (the previous log is rotated to `_1.log`).

**Fail modes**:
- Crash on RT dispatch → `setViewport(0, 0, nullptr)` invalid argument; check Vulkan loader version supports count-0 form
- Crash on RT pipeline create → if Part 1 was not applied, the v166 patch's pDynamicStates will still cause issues

### Step 7: Verify VUID-03602 is absent (Part 1 verification)
```bash
grep -c 'VUID-VkRayTracingPipelineCreateInfoKHR-pDynamicStates-03602' \
  Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log
```
**Expected**: 0 hits.

**Fail modes**:
- 1+ hits → Part 1 not applied OR new spec violation; recheck revert

### Step 8: Verify VUID-08608 is absent (Part 2 verification)
```bash
grep -c 'VUID-vkCmdTraceRaysKHR-None-08608' \
  Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log
```
**Expected**: 0 hits.

**Fail modes**:
- 1+ hits → Part 2's explicit-clear didn't resolve the underlying command-buffer pollution. Escalate to fallback path (modify `vulkan-graphics.cpp::commitGraphicsState` to skip setViewport/setScissor when next bind is RT).

### Step 9: Run validate_restir_gi.py on the new dump group
```bash
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
```
**Expected**: **6/6 PASS** (per tick986 validator upgrade — `validate_restir_gi.py main()` at line 336-338 now sums `ok1..ok6` and returns 0 only when all 6 pass; the 4-check list above (non-black, spatial std, cell variance, alpha sentinel) is now checks 1-4, with the 5th = `check_restir_alive` (spatial+denoised must have non-black channel means) and the 6th = `check_denoise_effective` (denoised_frame must differ from spatial_frame by MAE > 0.5 AND high-frequency energy must drop). The PENDING_TEST_AUDIT_v167.md §"Revision 2026-08-14 (tick986)" already documents this. Do NOT expect the legacy "4/4" — that text is stale.)

**Fail modes**:
- 0-3 PASS → something else broke; check the gi_raw stats from the new log:
  ```bash
  grep 'gi_raw floats' Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log
  ```
  Compare against the 23:51 `_2.log` baseline (R[0,2.032] G[0,1.908] B[0,2.070]).

### Step 10: Vision-verify the new display dump
```bash
xdg-open Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/$(ls -t \
  Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/*display*.png | head -1)
```
**Expected**: recognizable Sponza geometry with sane exposure.

**Fail modes**:
- Black or magenta → the GI shader is reading zero (GBuffer SRV still broken); suspect the GBuffer SRV layout issue from `DIAGNOSTIC_2026-07-30.md` is still present and unrelated to VUID-08608. This was the original user-named "GBuffer SRV binding fix" target.

## Test summary

**Coverage**: 7 of the 7 user-named acceptance criteria from the PICK card are addressed:
1. Debug target builds → Step 5
2. `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs cleanly → Step 6
3. No Vulkan VUID/ERROR → Steps 7-8
4. No command-list errors → Step 6 (binary completes without CommandList-error log lines)
5. `validate_restir_gi.py` PASS on newest dump group → Step 9
6. Fresh display image shows recognizable Sponza with sane exposure (vision) → Step 10
7. `HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial → Step 6 (binary dumps gi_raw PNGs with mode 20)

**Test verification recipe is operator-side terminal+python3+numpy+vision dependent. 0/7 criteria are cron-verifiable. File-only runspace reconfirmed.**

## Test output capture

Each successful run should produce:
- New `Binary/Debug/TestReSTIR_GI_Temporal.log` (rotated previous to `_1.log`)
- 8 new dump PNGs in `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/` prefixed `2026082[0-9]*`
- `validate_restir_gi.py` PASS output

These artifacts are the empirical evidence for the testing-verifier role to upgrade v167 to ALL_KEEP.
