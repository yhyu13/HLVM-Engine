# Pending Tests v136
- task: Revert v132 createValidationLayer hookup (build-unblocker)
- tester: tester (file-only single-profile mode)
- timestamp: 2026-07-30

## Test file

No new test file produced. v136 is a build-system patch, not a behavioral change. The actual behavior tests are deferred to the parent runspace.

## File-only verification (run in this turn, no terminal required)

1. **Patch applied**: `Engine/Source/Runtime/Private/Renderer/DeviceManagerVk4_LifeCycle.cpp:88` now has `m_ValidationLayer = nullptr;` (was `m_ValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);`).
2. **No dangling reference**: `search_files` for `createValidationLayer` returns only comment matches (in the v136 explanatory comment block).
3. **v131+v135 patches intact**: `FGIPass.cpp:557-562, 675` and `GIPathTracing.hlsl:685-687, 712-714` all present.
4. **v133+v134 cmake flags intact**: `Engine/Source/Runtime/CMakeLists.txt:182` still has `NVRHI_WITH_VALIDATION=ON`. `Build/Debug/_deps/nvrhi-src/CMakeLists.txt:209-214, 233-236` still has validation TUs in `add_library`.

## Behavioral tests (terminal+vision required, deferred to parent runspace)

The parent must:

1. Run `./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug` — should succeed (link error gone).
2. Run `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 ./Binary/Debug/TestReSTIR_GI_Temporal` — inspect gi_raw per-pixel via numpy.
3. If mode 20 returns non-zero per-pixel: v131+v135 was the fix; bisect closes.
4. If mode 20 still returns zero: v137 must investigate slangc dead-strip / pipeline cache staleness / Output UAV mis-bind / `VK_LAYER_KHRONOS_validation=1` env var.

## Test count

- File-only tests: 4 PASS (this turn)
- Behavioral tests: 0/4 runnable in this runspace (deferred)

---

**Per `six-role-pipeline §Role #5 (tester)`, this is a file-only test report. Behavioral tests deferred.**