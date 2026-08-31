# Pending Tests v51
- commit: docs/PENDING_COMMIT_v51.md
- tests: N/A — documentation-only tick (zero source-code lines modified)

## Test rationale
v51 is a pure structural standby tick with 0 source-code lines modified. There is no test surface to validate directly. The "test" for v51 is the parent-driven terminal-access invocation of `fresh-evidence-scan.sh`, which validates the cumulative 21-patch inventory is still intact and that the parent has fresh evidence to feed back.

## Part A — static-audit tests (cron-runnable; PASS this tick)
| # | Test | Path | Expected | Actual |
|---|------|------|----------|--------|
| A1 | Cumulative patch catalog in fresh-evidence-scan.sh CHECKS | TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh:60-86 | 27 entries (v3..v41 + bug-088 + bug-075) | 27 entries confirmed at lines 60-86 by read_file |
| A2 | v22 binding-layout-split UAVBindingLayout member | Public/Renderer/GI/FGIPass.h:106 | matches comment | matches (`// v22 split: separate layout for u0/u1 UAVs`) |
| A3 | v22 binding-layout-split SRVBindingSet addBindingSet | Private/Renderer/RayTracing/FRayTracingPipeline.cpp:357 | matches | matches (`State.addBindingSet(SRVBindingSet.Get())`) |
| A4 | v22 binding-layout-split UAVBindingSet addBindingSet | Private/Renderer/RayTracing/FRayTracingPipeline.cpp:361 | matches | matches (`State.addBindingSet(UAVBindingSet.Get())`) |
| A5 | v41 alpha-encoder std::clamp | Private/Image/FImageDump.cpp:27 | matches | matches (`std::clamp(rgbaData[i * 4 + 3] * 255.0f, 0.0f, 255.0f)`) |
| A6 | v38 cerr DebugMode effective= | Private/Renderer/GI/FGIPass.cpp:487 | matches | matches (`std::cerr << "[RGI] FGIPass::WriteConstants: DebugMode effective="`) |
| A7 | v17 case 7u both HLSL copies | data-dir/Private:604 | matches | matches (`case 7u: debugColor = diffuse * g_GI.AmbientColor.rgb * ambientScale;`) |
| A8 | v19 case 12u both HLSL copies | data-dir/Private:663 | matches | matches (`case 12u: debugColor = g_GI.AmbientColor.rgb;`) |
| A9 | v19 case 15u both HLSL copies | data-dir/Private:670 | matches | matches (`case 15u: debugColor = float3(g_GI.Params5.x, g_GI.Params5.x, g_GI.Params5.x);`) |
| A10 | v28 alpha sentinel both HLSL copies | data-dir/Private:694 | matches | matches (`Output[pixel].w = max(Output[pixel].w, 0.99994f);`) |
| A11 | v37 check_alpha_sentinel in validator | TestReSTIR_GI_Temporal_Data/validate_restir_gi.py:134 | matches | matches (`def check_alpha_sentinel(files, saturated_min=0.95, low_max=0.95):`) |
| A12 | v40 v40-alpha block in dump_pixelstats | TestReSTIR_GI_Temporal_Data/dump_pixelstats.py:184 | matches | matches (`# Map verdict -> human-readable diagnostic line that mirrors...`) |
| A13 | v22 UAVBindingLayout init at FGIPass.cpp:183 | Private/Renderer/GI/FGIPass.cpp:183 | matches | matches (`UAVBindingLayout = nullptr; // v22 split: clear separate UAV layout`) |
| A14 | v22 UAVBindingLayout createBindingLayout at FGIPass.cpp:311 | Private/Renderer/GI/FGIPass.cpp:311 | matches | matches (`UAVBindingLayout = Device->createBindingLayout(UAVLayoutDesc);`) |

14/14 Part A tests PASS via static inspection (search_files + read_file).

## Part B — runtime tests (parent-driven; cron terminal blocked; PENDING until v52+)
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
14/14 Part A static tests PASS. 0/8 Part B runtime tests PASS (cron terminal blocked; parent must run). v51 is structurally identical to v25-v50 — documentation-only tick, no test surface change. Verdict: PENDING parent-evidence-gated continuation.
