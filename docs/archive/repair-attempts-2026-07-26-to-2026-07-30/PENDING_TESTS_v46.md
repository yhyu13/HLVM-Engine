# Pending Tests v46
- task: Structural standby tick — file-only re-audit.

## Part A: Static tests (cron-executable; PASS via search_files + read_file)
1. **PATCH_INVENTORY_INTACT** — `search_files pattern="LogGI:|LogTest:" path="Engine/Source/Runtime"` returns ≥5 v3 spdlog-marker matches in FGIPass.cpp + TestReSTIR_GI_Temporal.cpp.
2. **V12_CERR_DEFAULT_ON** — `search_files pattern="\[RGI\] (Render|FGIPass::DispatchRays) entry:"` returns ≥2 matches (one in each C++ file).
3. **V41_ALPHA_ENCODER_FIX** — `search_files pattern="rgbaData\[i\*4\+3\] \* 255" path="Engine/Source/Runtime/Private/Renderer/Image/FImageDump.cpp"` returns ≥1 match.
4. **V37_VALIDATOR_ALPHA_CHECK** — `search_files pattern="check_alpha_sentinel" path="Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py"` returns ≥1 match.
5. **V22_BINDING_LAYOUT_SPLIT** — `search_files pattern="addBindingSet" path="Engine/Source/Runtime/Private/Renderer/GI/FRayTracingPipeline.cpp"` returns ≥1 match; `search_files pattern="UAVBindingLayout" path="Engine/Source/Runtime/Private/Renderer/GI/FGIPass.h"` returns ≥1 match.
6. **V13_V15_CASE6U_HLSL_SYNC** — `search_files pattern="case 6u:" path="Engine/Source/Runtime"` returns ≥2 matches (Private master + data-dir copy).
7. **NO_FABRICATED_EVIDENCE** — `search_files pattern="PIPELINE_GOAL_DONE"` returns 0 matches (final-goal gate still FAILED/UNVERIFIED).

## Part B: Runtime tests (parent-driven; PENDING — terminal blocked by tirith in this cron tick)
B1. **BUILD_CLEAN** — `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` exits 0.
B2. **FRESH_RUN_WITH_CERR** — `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` produces ≥8 `[RGI] Render() entry:` + ≥8 `[RGI] FGIPass::DispatchRays() entry:` cerr lines.
B3. **MODE6_PIXEL_GRADIENT** — Same with `HLVM_PT_DEBUG_MODE=6`; `gi_raw` PNG shows per-pixel gradient `(R=x/256, G=0, B=y/256)`.
B4. **ALPHA_SENTINEL_SATURATED** — `display_frame8.png` alpha channel ≥95% pixels at 254-255 (proves v28+v37+v41 chain).
B5. **VALIDATOR_PASS** — `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` exits 0 with 3/3 (now 4/4 with v37 alpha-check).
B6. **VISION_ANALYSIS_PASS** — `display_frame8.png` shows recognizable, non-uniform Sponza geometry with sane exposure.