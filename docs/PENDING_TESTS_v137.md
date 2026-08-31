# Pending Tests v137
- task: Add `setBindingOffsets(0,0,0,0)` to FGIPass's UAV-only binding layout (descriptor-slot double-add bug fix)
- tester: tester (file-only single-profile mode)
- timestamp: 2026-07-31

## Test file

No new test file produced. v137 is a binding-bug patch, not a behavioral change. The actual behavior tests are deferred to the parent runspace.

## File-only verification (run in this turn, no terminal required)

1. **Patch applied**: `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:313-318` now contains:
   ```cpp
   nvrhi::VulkanBindingOffsets UAVOffsets;
   UAVOffsets.setConstantBufferOffset(0)
             .setShaderResourceOffset(0)
             .setSamplerOffset(0)
             .setUnorderedAccessViewOffset(0);
   UAVLayoutDesc.setBindingOffsets(UAVOffsets);
   ```
   Read back the file (lines 295-335) — confirmed via search_files.
2. **No dangling reference**: `search_files` for `UAVLayoutDesc` returns only the 4 expected matches in FGIPass.cpp (lines 301, 310, 311, 318 after patch).
3. **v131+v135 patches intact**: `FGIPass.cpp:557-562` (v135 commitBarriers) and `FGIPass.cpp:675` (v131 commitBarriers) present.
4. **v132+v133+v134+v136 patches intact**: `DeviceManagerVk4_LifeCycle.cpp:88, 163` have `m_ValidationLayer = nullptr;` (v136 revert). `Engine/Source/Runtime/CMakeLists.txt:182` has `NVRHI_WITH_VALIDATION=ON`. `Build/Debug/_deps/nvrhi-src/CMakeLists.txt:209-214, 233-236` has validation TUs in `add_library`.
5. **GIPathTracing.hlsl debug modes intact**: cases 20u/21u/22u/30u/31u at lines 685-687, 712-714 present.

## Behavioral tests (terminal+vision required, deferred to parent runspace)

The parent must:
1. Run `./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug` — should succeed (no link error after v136 revert).
2. Run `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=6 ./Binary/Debug/TestReSTIR_GI_Temporal` — expect per-pixel gradient in `gi_raw_frame8.png` (red gradient on x-axis from 0..1, blue gradient on y-axis from 0..1, green=0).
3. Run `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 ./Binary/Debug/TestReSTIR_GI_Temporal` — expect non-zero per-pixel GBufferMaterial values matching the staging dump of `gbuffer_material_frame8.png`.
4. Run `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal` — expect the gi_raw dump to show actual path-trace GI output (not all-zero).
5. If all 4 produce expected results: bisect closes, run `validate_restir_gi.py` on the fresh dump group.
6. If mode 6 shows the per-pixel gradient AND mode 20 still returns zero: v137 fixed the UAV write but mode 20 has a SEPARATE root cause. v138 will investigate (a) image layout transition for first-frame SRV reads, (b) slangc dead-strip of cases 20/21/22 (already falsified by v24 spirv-dis), (c) Output UAV mis-bind.
7. If mode 6 still returns zero (no gradient): v137 didn't fix the UAV write bug. v138 will need to investigate (a) image layout transition for OutputTexture, (b) descriptor set creation failure, (c) Output UAV not bound to the right texture.

## Test count

- File-only tests: 5 PASS (this turn)
- Behavioral tests: 0/7 runnable in this runspace (deferred)

---

**Per `six-role-pipeline §Role #5 (tester)`, this is a file-only test report. Behavioral tests deferred.**