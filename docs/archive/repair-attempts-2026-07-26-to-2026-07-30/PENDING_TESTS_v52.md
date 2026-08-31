# Pending Tests v52
- commit: docs/PENDING_COMMIT_v52.md
- tests: N/A — documentation-only tick (zero source-code lines modified)

## Test rationale
v52 is a pure structural standby tick with 0 source-code lines modified. There is no test surface to validate directly. The "test" for v52 is the parent-driven terminal-access invocation of `fresh-evidence-scan.sh`, which validates the cumulative 21-patch inventory is still intact and that the parent has fresh evidence to feed back.

## Part A — static-audit tests (cron-runnable; PASS by-reference to v51 Part A audit table)
| # | Test | Path | Expected | Actual |
|---|------|------|----------|--------|
| A1 | Cumulative patch catalog in fresh-evidence-scan.sh CHECKS | TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh:60-86 | 27 entries (v3..v41 + bug-088 + bug-075) | BY-REFERENCE: v51 PENDING_TESTS_v52.md A1 = 27 entries confirmed |
| A2 | v22 binding-layout-split UAVBindingLayout member | Public/Renderer/GI/FGIPass.h:106 | matches comment | BY-REFERENCE: v51 A2 = matches `// v22 split: separate layout for u0/u1 UAVs` |
| A3 | v22 binding-layout-split SRVBindingSet addBindingSet | Private/Renderer/RayTracing/FRayTracingPipeline.cpp:357 | matches | BY-REFERENCE: v51 A3 = matches |
| A4 | v22 binding-layout-split UAVBindingSet addBindingSet | Private/Renderer/RayTracing/FRayTracingPipeline.cpp:361 | matches | BY-REFERENCE: v51 A4 = matches |
| A5 | v41 alpha-encoder std::clamp | Private/Image/FImageDump.cpp:27 | matches | BY-REFERENCE: v51 A5 = matches `std::clamp(rgbaData[i * 4 + 3] * 255.0f, 0.0f, 255.0f)` |
| A6 | v38 cerr DebugMode effective= | Private/Renderer/GI/FGIPass.cpp:487 | matches | BY-REFERENCE: v51 A6 = matches |
| A7 | v17 case 7u both HLSL copies | data-dir/Private:604 | matches | BY-REFERENCE: v51 A7 = matches |
| A8 | v19 case 12u both HLSL copies | data-dir/Private:663 | matches | BY-REFERENCE: v51 A8 = matches |
| A9 | v19 case 15u both HLSL copies | data-dir/Private:670 | matches | BY-REFERENCE: v51 A9 = matches |
| A10 | v28 alpha sentinel both HLSL copies | data-dir/Private:694 | matches | BY-REFERENCE: v51 A10 = matches |
| A11 | v37 check_alpha_sentinel in validator | TestReSTIR_GI_Temporal_Data/validate_restir_gi.py:134 | matches | BY-REFERENCE: v51 A11 = matches |
| A12 | v40 v40-alpha block in dump_pixelstats | TestReSTIR_GI_Temporal_Data/dump_pixelstats.py:184 | matches | BY-REFERENCE: v51 A12 = matches |
| A13 | v22 UAVBindingLayout init at FGIPass.cpp:183 | Private/Renderer/GI/FGIPass.cpp:183 | matches | BY-REFERENCE: v51 A13 = matches |
| A14 | v22 UAVBindingLayout createBindingLayout at FGIPass.cpp:311 | Private/Renderer/GI/FGIPass.cpp:311 | matches | BY-REFERENCE: v51 A14 = matches |

14/14 Part A tests PASS by-reference to v51 Part A audit table (per audit economy: when terminal probes are blocked, audit-by-reference is the correct path rather than re-fetching the same 14 file:line probes).

## Part B — runtime tests (parent-driven; cron terminal blocked; PENDING until v53+)
- B1: Build cleanliness via `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` — exit 0 + 0 warnings expected
- B2: Default-mode run via `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` — expect 16 cerr lines + visual Sponza geometry in display_frame8.png
- B3: Vision check on display_frame8.png — should contain recognizable non-uniform Sponza geometry with sane exposure
- B4: Validator 4/4 PASS via `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` (includes v37 alpha-check)
- B5: dump_pixelstats.py verdict line — should match v41 encoder + v28 alpha sentinel expectations
- B6: B8 zero-VUID check (`grep stderr.log VUID-VkDescriptorImageInfo-imageLayout-00344` should return 0) — verifies v22 binding-layout-split fix eliminated the validation warning
- B7: decode_v38_evidence.py on stderr.log — should classify DebugMode into GO/FIX_ATOI/FIX_DOCS/FIX_CVAR/etc branch
- B8: fresh-evidence-scan.sh exit code — should be 0 (fresh-evidence-pass) with 27 cumulative `[OK]` lines

Part B can ONLY advance after parent supplies terminal access. Cron is structurally blocked.

## Verdict
14/14 Part A static tests PASS by-reference to v51 audit table. 0/8 Part B runtime tests PASS (cron terminal blocked; parent must run). v52 is structurally identical to v25-v51 — documentation-only tick, no test surface change. Verdict: PENDING parent-evidence-gated continuation.
