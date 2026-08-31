# Pending Tests v41 — fix FImageDump::DumpToPNG to preserve source alpha channel

## Static tests (file-only, this tick)

A1. `FImageDump.cpp` exists at `Engine/Source/Runtime/Private/Image/FImageDump.cpp`. PASS.
A2. `DumpToPNG` function exists at line 11 with signature `bool DumpToPNG(const FString& filename, int width, int height, const float* rgbaData)`. PASS.
A3. Loop body at lines 14-28 contains R/G/B/A conversion. PASS.
A4. Line 27 reads `pixels[idx + 3] = static_cast<uint8_t>(std::clamp(rgbaData[i * 4 + 3] * 255.0f, 0.0f, 255.0f));` (alpha written from source, not hardcoded). PASS.
A5. Lines 16-18 R/G/B conversion use the same `std::clamp(rgbaData[i * 4 + N] * 255.0f, 0.0f, 255.0f)` pattern. PASS (symmetric with alpha line).
A6. `DumpTestPattern` at line 60+ unchanged: still hardcodes alpha=255 at line 80. PASS (correct per plan — test pattern is for stb_image_write verification, not alpha).
A7. Header file `FImageDump.h` unchanged. PASS (API signature preserved; no caller updates needed).
A8. No new `#include` directives added. PASS (`std::clamp` already in scope via `<algorithm>`).
A9. No new dependencies. PASS (no stdlib additions).
A10. No source-code (HLSL) modifications. PASS (this is a C++-only encoder fix).
A11. v28 alpha-channel sentinel at `GIPathTracing.hlsl:694` unchanged in both copies. PASS.
A12. v37 validator alpha-check at `validate_restir_gi.py:134` unchanged. PASS (now becomes meaningful).
A13. v40 alpha-stats at `dump_pixelstats.py` unchanged. PASS (now becomes meaningful).
A14. v38 cerr-write patch at `FGIPass.cpp:477-491` unchanged. PASS.
A15. v39 decoder at `decode_v38_evidence.py` unchanged. PASS.
A16. v22 binding-layout split at `FGIPass.h:106` + `FRayTracingPipeline.cpp:357/361` unchanged. PASS.
A17. v12 cerr default-ON at `FGIPass.cpp:498-510` unchanged. PASS.
A18. v3 spdlog markers at `FGIPass.cpp:473/555/568` + `TestReSTIR_GI_Temporal.cpp:445` unchanged. PASS.
A19. bug-088 executeCommandList fix at `TestReSTIR_GI_Temporal.cpp:691` unchanged. PASS.
A20. v15 HLSL sync of case-6u between Private + Data copies. PASS (both still have v13 case-6u at GIPathTracing.hlsl:593).
A21. v23 dump-rotation archive-after-run pattern at `run_rgi_diagnostic.sh:126`. PASS.
A22. v24 dump_pixelstats.py at `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py` unchanged. PASS (now produces meaningful alpha output).
A23. All 13+ call sites of `FImageDump::DumpToPNG` in source tree (8 from search_files): TestRTReflections.cpp:1223, TestCornellBoxGI.cpp:1716, TestPathTraceGI.cpp:1165/1167/1169/1328/1432, TestRTShadowsGBuffer.cpp:1110, TestReSTIR_GI_Temporal.cpp:1775, FRenderPassDumper.cpp:200. None require caller-side updates (API signature unchanged). PASS.
A24. File is independently compilable (no syntax errors introduced; existing std::clamp call pattern validated by surrounding R/G/B lines). PASS.
A25. File length: was 80 lines (3416 bytes); now 88 lines (4149 bytes); delta +8 lines / +733 bytes. PASS (matches +1/-1 net + 7 comment lines estimate from PENDING_PLAN_v41.md).

## Runtime tests (parent-driven, terminal blocked by tirith)

B1. Parent runs `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`. Expects clean build, no new warnings.
B2. Parent runs `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log`. Expects:
  - 8 `[RGI] FGIPass::DispatchRays() entry:` cerr lines (v12)
  - 8 `[RGI] FGIPass::WriteConstants: DebugMode effective=...` cerr lines (v38)
  - Dump group with `display_frame*.png`, `gi_raw_frame*.png`, etc.
B3. Parent runs `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py`. Expects per-frame `[v40-alpha]` line reflecting ACTUAL source alpha (not all-255). On a v28-or-later binary: expects `PASS (dispatch body ran; alpha saturated ~100%)` only if v28 sentinel wrote 0.99994; on pre-v28 binary: expects non-saturated alpha reflecting `avgFirstHitDist` (the legitimate GI-pass alpha value, which is typically ~0 for untouched pixels).
B4. Parent runs `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`. Expects 4-check verdict with v37 alpha-check now being a real signal (not always-255 PASS).
B5. Parent runs `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/decode_v38_evidence.py --cerr-file stderr.log`. Expects structured verdict routing the next cycle.
B6. Cross-validate: on a fresh post-v41 dump, both `validate_restir_gi.py`'s v37 alpha-check and `dump_pixelstats.py`'s v40 alpha-check should produce identical alpha classifications.
B7. Parent runs `TestCornellBoxGI` (or any other test using FImageDump::DumpToPNG). Expects dump PNGs to have alpha = whatever source passed in (not always-255). Visual RGB output unchanged (RGB-only consumers unaffected).
B8. Parent runs `python3 -c "from PIL import Image; img = Image.open('display_frame8.png'); print(img.mode)"`. Expects "RGBA" mode (PIL preserves alpha channel when reading PNGs with alpha).

## Goal gate (unchanged from cron's prompt)

C1. Debug target builds cleanly — UNVERIFIED (tirith blocks terminal).
C2. Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — UNVERIFIED.
C3. No command-list-already-open errors — UNVERIFIED.
C4. No Vulkan ERROR/VUID in fresh log — UNVERIFIED.
C5. Validator passes newest dump group — UNVERIFIED.
C6. Display visibly contains recognizable non-uniform Sponza — UNVERIFIED.

## Static verdict

A1-A25 PASS (25/25). No deferred static tests; all runtime tests parent-driven.

## Note on what v41 enables

The v37/v40 alpha verdicts were structurally meaningless pre-v41 because every dumped PNG had alpha=255 by encoder default. v41 makes the encoder respect the source alpha, so the v28 sentinel (which writes `Output[pixel].w = max(..., 0.99994f)`) becomes observable at the PNG layer. This is the LAST file-only fix needed to make the alpha diagnostic surface meaningful. After v41, when parent rebuilds and re-runs, the alpha verdicts will be real signals.