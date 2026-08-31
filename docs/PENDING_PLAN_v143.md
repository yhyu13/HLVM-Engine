# Pending Plan v143
- task: v143 — close the remaining acceptance-evidence gaps for `TestReSTIR_GI_Temporal`: enable Vulkan debug validation before instance creation and make `validate_restir_gi.py` evaluate only the newest dump group.
- source: `docs/DIAGNOSTIC_2026-07-30.md`, the post-v142 logs at `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal{,_1,_2}.log`, and direct source inspection.
- approach: Move the test's `bEnableDebugRuntime` request to the pre-creation parameter block and explicitly enable the NVRHI validation wrapper there, because setting it after `CreateWindowDeviceAndSwapChain()` cannot enable the Vulkan layer or messenger. Add a small pure-Python newest-group selector to the validator, anchored on the maximum `display_frame8` timestamp, plus focused module-direct regression tests, so stale flat-directory dumps cannot satisfy any check. Touch only test/harness files.
- diff_estimate: +105 / -8 lines across 3 existing/new test files
- skip_plan_review: no
- test_strategy: Add a module-direct Python test for newest-group selection and source-order assertions for the pre-device validation flags; then run the Debug target, default dump command, newest-group validator, mode 20, fresh log scan, and image inspection when terminal/vision are available.
- risks: The system Vulkan validation layer may be unavailable, in which case device creation will fail with explicit missing-layer evidence rather than silently running unvalidated. Grouping by the maximum display timestamp assumes display is dumped first for each run (the current C++ dump order); a future order change must update the selector. The scheduled runspace still cannot execute terminal or vision, so behavioral acceptance remains externally blocked until a capable run runs the recipe.

## Root-cause evidence

1. `TestReSTIR_GI_Temporal.cpp` previously set `Params.bEnableDebugRuntime = true` only **after** `CreateWindowDeviceAndSwapChain()`. `DeviceManagerVk1_Instance.cpp:75-79,179-193` consumes this flag while creating the Vulkan instance, layer list, and debug messenger, so the late write is a no-op. This explains the fresh log's empty `Enabled Vulkan layers:` section. The v143 block follows the proven pre-creation setup in `TestPathTraceGI.cpp:1509-1520`.
2. `DeviceManagerVk4_LifeCycle.cpp:23-26,82-119` consumes `bEnableNVRHIValidationLayer` during device creation. Setting it explicitly before creation makes the intended v139 wrapper activation unambiguous instead of depending only on a CVar.
3. `validate_restir_gi.py:97-108` previously loaded every historical `*frame8.png`; display checks then selected the first lexicographic display, violating the user requirement to validate the newest group only.
4. Post-v142 default-mode logs already show matching raster/GI handles, t1/t2/t3 descriptors, successful dispatch, and non-zero varying GI output. No additional speculative binding mutation is justified before the required fresh mode-20 run.

## Behavioral verification

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
python3 -m unittest discover \
  -s Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data \
  -p 'test_validate_restir_gi.py' -v
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 ./Binary/Debug/TestReSTIR_GI_Temporal
```

Acceptance after those commands: unit tests pass; build succeeds; log lists `VK_LAYER_KHRONOS_validation`; no VUID/ERROR/command-list errors; validator reports 4/4 using only files at/after the newest display timestamp; mode 20 gi_raw is non-zero; newest default-mode display is recognizable Sponza with sane exposure.
