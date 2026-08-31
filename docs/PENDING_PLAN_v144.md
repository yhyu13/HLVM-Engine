# Pending Plan v144
- task: v144 — Make the NVRHI validation-wrapper linkage durable so the v143 pre-device validation configuration can build.
- source: no bundle — direct edit; authoritative evidence is `Engine/Source/Runtime/rebuild_Debug.log:147-153`, generated `Engine/Source/Runtime/Build/Debug/build.ninja:3944-3960`, `Runtime_cmake.py:121-126`, and generated `Runtime/CMakeLists.txt:270-273`.
- approach: Replace the plain `nvrhi` dependency in the PyCMake FetchContent binding with CMake's supported `$<LINK_LIBRARY:WHOLE_ARCHIVE,nvrhi>` generator expression, and apply the equivalent generated Runtime CMake change so the validation-device object cannot be discarded while resolving `createValidationLayer`. Add a focused source-contract regression test to the existing validator test module; do not change the renderer binding layout or disable Vulkan validation. Touch only the build-source/generated CMake linkage and the focused Python test.
- diff_estimate: +18 / -2 lines across 3 files
- skip_plan_review: no
- test_strategy: Extend `test_validate_restir_gi.py` with read-only assertions that both the PyCMake source and generated Runtime CMake retain the whole-archive expression and that the ordinary `nvrhi` dependency is not the sole linkage form; then, when terminal is available, rebuild the Debug target and run the complete v143 behavioral recipe.
- risks: CMake versions older than 3.24 do not support `LINK_LIBRARY`; this project’s generated build uses CMake 3.29.3, but the source-contract test must document that requirement. Whole-archiving the common NVRHI archive may pull additional objects and expose unrelated duplicate symbols; the existing link uses `-Wl,-allow-multiple-definition`, so the reviewer must inspect generated ordering and reject scope creep. If the symbol remains unresolved, the next fix must inspect the archive symbol table in a terminal-capable runspace rather than guessing. Runtime acceptance remains unverified while tirith denies terminal and vision is unavailable.

## Root-cause evidence

1. The latest build log reaches `[114/114]` and fails only at the executable link with `undefined reference to nvrhi::validation::createValidationLayer(nvrhi::IDevice*)` (`rebuild_Debug.log:147-153`).
2. The current generated Ninja graph lists `validation-device.cpp.o` in the `libnvrhid.a` archive (`build.ninja:3944,3960`), so simply re-adding the source-list entry is not an adequate new hypothesis.
3. The executable link consumes `libRuntimed.a`, `libnvrhi_vkd.a`, and `libnvrhid.a` as static archives; forcing the common archive's members is the smallest durable linker-level probe that distinguishes archive extraction/LTO visibility from a missing source entry.
4. `Runtime_cmake.py:121-126` is the source of the generated dependency line; `Runtime/CMakeLists.txt:270-273` is the current generated realization. Both must agree to avoid regeneration reverting the fix.

## Verification

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
python3 -m unittest discover -s Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data -p 'test_validate_restir_gi.py' -v
./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 ./Binary/Debug/TestReSTIR_GI_Temporal
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
```

Acceptance for this cycle: source-contract tests pass; Debug link succeeds; a fresh default and mode-20 run produce new logs/dumps; the fresh log contains `VK_LAYER_KHRONOS_validation` and no VUID/ERROR/command-list errors; newest-group validator reports 4/4; mode 20 `gi_raw` is non-zero; and the newest default display is visually recognizable Sponza with sane exposure. If the build remains blocked, record the exact linker output and do not claim runtime success.
