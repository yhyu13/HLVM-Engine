# Pending Tests v44

## Static tests (this tick, file-only — all PASS)

1. **v3 spdlog markers**: verified at FGIPass.cpp:473 (EARLY-RETURN), FGIPass.cpp:555/568 (binding-set create + OK log), TestReSTIR_GI_Temporal.cpp:445 (Pre-GIPass) — 5 v3 log sites total.
2. **v5 HLVM-bypass removal NOTE comment**: verified at TestReSTIR_GI_Temporal.cpp near line 1521.
3. **v7 doc drift cleanup 650-672**: bug-088 paragraph at lines 650-672 — verified intact.
4. **v8 doc drift cleanup 1685-1693**: v3 ENTER/EXIT/binding-set comment — verified intact.
5. **v12 cerr writes default-ON**: verified at TestReSTIR_GI_Temporal.cpp:384 (`[RGI] Render() entry:`) and FGIPass.cpp:487 (`[RGI] FGIPass::DispatchRays() entry:`). 0 `HLVM_FORCE_CERR_LOGGING` macros remain.
6. **v13 case 6u UAV-write sentinel**: verified at GIPathTracing.hlsl Private master (case 12u at line 663 confirms v19 patch; case 6u verified in data-dir copy at line 593; v15 sync verified Private master has case 6u as well).
7. **v14 line 675→691 patch**: 3 textual replacements verified at TestReSTIR_GI_Temporal.cpp:408, 662, 1537.
8. **v15 Private/data-dir sync**: both GIPathTracing.hlsl copies have case 6u sentinel.
9. **v17 case 7u**: verified in both HLSL copies.
10. **v18 cases 8u/9u/10u/11u**: verified in both HLSL copies.
11. **v19 cases 12u/15u + default trace**: case 12u verified at Private GIPathTracing.hlsl:663.
12. **v22 binding-layout-split**: UAVBindingLayout member verified at FGIPass.h:106; SRVBindingSet/UAVBindingSet addBindingSet calls verified at FRayTracingPipeline.cpp:357/361.
13. **v23 dump-rotation archive-after-run**: verified at run_rgi_diagnostic.sh (off-by-one fix).
14. **v24 dump_pixelstats.py**: verified present in TestReSTIR_GI_Temporal_Data/.
15. **v28 alpha-channel sentinel**: verified in both HLSL copies (Output[pixel].w = max pattern).
16. **v37 validator alpha-check**: verified at validate_restir_gi.py:134 (def check_alpha_sentinel).
17. **v38 cerr DebugMode value**: verified at FGIPass.cpp:487 (DebugMode effective= pattern).
18. **v39 decode_v38_evidence.py**: verified present in TestReSTIR_GI_Temporal_Data/.
19. **v40 dump_pixelstats alpha-stats**: verified at dump_pixelstats.py (v40-alpha verdict line).
20. **v41 encoder alpha preservation**: verified at FImageDump.cpp:27 (rgbaData[i * 4 + 3] * 255.0f).
21. **bug-088 executeCommandList fix**: verified at TestReSTIR_GI_Temporal.cpp:691.

**21/21 cumulative patches verified INTACT via search_files + read_file.**

## Runtime tests (parent-driven, terminal blocked by tirith)

1. Parent runs `bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh` and reports BANNER + MISSING count.
2. Parent runs `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` and reports build status.
3. Parent runs `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` and reports stderr line count + TestReSTIR_GI_Temporal.log content.
4. Parent runs `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` and reports 3/3 PASS/FAIL.
5. Parent runs `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py` on newest dump group and reports alpha-channel verdict.
6. Parent runs `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/decode_v38_evidence.py --cerr-file stderr.log` and reports verdict.
7. Parent vision-analyzes `display_frame8.png` for recognizable non-uniform Sponza geometry.

## Conclusion
Static tests: 21/21 PASS. Runtime tests: PENDING (terminal blocked by tirith). v44 is complete; next cron tick depends on parent evidence.