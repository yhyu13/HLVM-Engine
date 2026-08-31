# Pending Tests v45

- commit: docs/PENDING_COMMIT_v45.md
- timestamp: 2026-07-27

## Part A: Static tests (this tick, file-only, executable via search_files + read_file)

| # | Test | Method | Expected | Status |
|---|------|--------|----------|--------|
| A1 | v3 spdlog markers in source | `read_file` at FGIPass.cpp:486, TestReSTIR_GI_Temporal.cpp:445 | "ENTER" and "Pre-GIPass" markers present | [OK] |
| A2 | v12 cerr writes default-ON | `read_file` at TestReSTIR_GI_Temporal.cpp:384, FGIPass.cpp:487 | "std::cerr" lines present (not gated by HLVM_FORCE_CERR_LOGGING) | [OK] |
| A3 | v13/v15 case 6u in both HLSL copies | `search_files` for "case 6u" in Private master + data-dir | Both copies contain case 6u sentinel | [OK] |
| A4 | v14 line-691 references | `search_files` for "line 691" in TestReSTIR_GI_Temporal.cpp | 3 matches at lines 408, 662, 1537 | [OK] |
| A5 | v22 binding-layout-split | `search_files` for "UAVBindingLayout" + "SRVBindingSet" + "UAVBindingSet" in FGIPass.h/cpp + FRayTracingPipeline.cpp | All 6 sites present | [OK] |
| A6 | v28 alpha sentinel in both HLSL copies | `search_files` for "0.99994" in Private master + data-dir | Both copies contain alpha write | [OK] |
| A7 | v37 validator alpha-check | `search_files` for "check_alpha_sentinel" in validate_restir_gi.py | 1 match at line 134 | [OK] |
| A8 | v38 cerr DebugMode value | `read_file` at FGIPass.cpp:487-491 | "DebugMode effective=" cerr line present | [OK] |
| A9 | v39 decode_v38_evidence.py | `search_files` for "decode_v38_evidence.py" | 1 file at TestReSTIR_GI_Temporal_Data/ | [OK] |
| A10 | v40 dump_pixelstats alpha-stats | `read_file` at dump_pixelstats.py | "v40-alpha" verdict line present | [OK] |
| A11 | v41 FImageDump encoder alpha | `read_file` at FImageDump.cpp:19-27 | "rgbaData[i*4+3] * 255.0f" present | [OK] |
| A12 | bug-088 fix | `read_file` at TestReSTIR_GI_Temporal.cpp:691 | "executeCommandList" call present | [OK] |
| A13 | All 21 cumulative patches | sum of A1-A12 + v5/v7/v8/v11/v17/v18/v19/v23/v24 | All 21 documented sites intact | [OK] |

## Part B: Runtime tests (parent-driven, terminal blocked by tirith)

| # | Test | Parent command | Expected |
|---|------|----------------|----------|
| B1 | fresh-evidence-scan.sh | `bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh` | exit 0 + banner "fresh-build-evidence-PASS" if dumps <1h, else "evidence-stale-or-missing" |
| B2 | validator on newest dump group | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` | 3/3 status (RGB) + alpha-channel check |
| B3 | alpha-channel pixel stats | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py` | v40-alpha verdict line |
| B4 | v38 cerr decode | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/decode_v38_evidence.py --cerr-file stderr.log` | verdict / branch / next-action |
| B5 | mode-6 UAV-write sentinel | `HLVM_PT_DEBUG_MODE=6 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>/dev/null` | gi_raw shows per-pixel gradient (0..3, 0, 0..2) if dispatch body runs |
| B6 | vision analysis | vision-analyze `display_frame8.png` | recognizable non-uniform Sponza geometry with sane exposure |

## Final test verdict
Part A: 13/13 PASS (file-only, executed this tick). Part B: PENDING (terminal blocked by tirith).
