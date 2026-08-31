# Pending Tests v68
- tests: docs/PENDING_TESTS_v68.md
- commit: docs/PENDING_COMMIT_v68.md

## Part A — Static probes (file-only, cron-executable)

| ID | Patch | Site | Expected | This tick |
|----|-------|------|----------|-----------|
| A1 | v22 UAVBindingLayout member | Public/Renderer/GI/FGIPass.h:106 | `nvrhi::BindingLayoutHandle UAVBindingLayout;` | PASS (1 hit) |
| A2 | v22 SRVBindingSet+UAVBindingSet 2-overload DispatchRays | Private/Renderer/RayTracing/FRayTracingPipeline.cpp:345/375/381 | 3 sites intact (signature + addBindingSet usage + recursive call) | PASS (5 hits, 3 expected sites match) |
| A3 | v41 std::clamp alpha-encoder | Private/Image/FImageDump.cpp:27 | `pixels[idx + 3] = static_cast<uint8_t>(std::clamp(rgbaData[i * 4 + 3] * 255.0f, ...));` | PASS (1 hit) |
| A4 | v38 cerr DebugMode effective= | Private/Renderer/GI/FGIPass.cpp:487 | `std::cerr << "[RGI] FGIPass::WriteConstants: DebugMode effective=" << DebugMode` | PASS (1 hit) |
| A5 | v13 case 6u UAV-write sentinel | Private/Renderer/Shader/GI/GIPathTracing.hlsl:593 + TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl:593 | `case 6u:  debugColor = float3(float(pixel.x) / 256.0, 0.0, float(pixel.y) / 256.0); break;` | PASS (2 hits, byte-identical) |
| A6 | v17 case 7u TraceRay-bypass sentinel | BOTH GIPathTracing.hlsl copies at :604 | `case 7u:  debugColor = diffuse * g_GI.AmbientColor.rgb * ambientScale; break;` | PASS (2 hits, byte-identical) |
| A7 | v28 alpha-channel alive-sentinel | BOTH GIPathTracing.hlsl copies at :694 | `Output[pixel].w = max(Output[pixel].w, 0.99994f);` | PASS (4 grep hits for "0.99994f" total: 2 sentinel-write sites + 2 comment-line mentions; the 2 write sites at :694 are correct) |

**Summary: 7/7 Part A static probes PASS.** Cumulative 22-patch inventory verified intact at start of tick.

## Part B — Runtime probes (PENDING, parent-driven, terminal blocked by tirith)

| ID | Test | Command | Expected | Status |
|----|------|---------|----------|--------|
| B1 | Build cleanliness | `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` | 0 errors, 0 new warnings, target links | PENDING (terminal blocked) |
| B2 | Default-mode run + stderr capture | `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` | stderr.log contains 8 `[RGI] Render() entry:` + 8 `[RGI] FGIPass::DispatchRays() entry:` + 8 `[RGI] FGIPass::WriteConstants: DebugMode effective=N ...` lines (v12 cerr default-ON + v38 cerr value-log) | PENDING |
| B3 | stderr VUID-00344 check | `grep -c VUID-VkDescriptorImageInfo-imageLayout-00344 stderr.log` | 0 hits (v22 binding-layout-split should eliminate the warning that v22 was designed to fix) | PENDING |
| B4 | Alpha-channel alpha-blit inspection | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py $DUMP_DIR/gi_raw_0.png` | alpha channel saturated 254-255 if v28 sentinel wrote 0.99994f + v41 encoder fix preserved source alpha | PENDING |
| B5 | Mode 6 UAV-write sentinel | `HLVM_PT_DEBUG_MODE=6 ./TestReSTIR_GI_Temporal` + inspect `gi_raw_*` | per-pixel gradient (R=x/256, G=0, B=y/256) — proves dispatch body + UAV write intact | PENDING |
| B6 | Mode 7 TraceRay-bypass | `HLVM_PT_DEBUG_MODE=7 ./TestReSTIR_GI_Temporal` + inspect `gi_raw_*` | `diffuse * g_GI.AmbientColor.rgb * ambientScale` = scene-shape × 1.5 — proves main-loop math + SRV reads intact | PENDING |
| B7 | Validator verdict | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py --dumps $DUMP_DIR` | 4/4 PASS (3 original checks + v37 alpha-sentinel check) | PENDING |
| B8 | Decoder verdict | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/decode_v38_evidence.py --cerr-file stderr.log` | structured verdict (GO/FIX_ATOI/FIX_DOCS/FIX_CVAR/NO_CERR/MIXED/UNRECOGNIZED) per v38 cerr-line shape | PENDING |

## Per-test verdict
N/A — Part A 7/7 PASS; Part B 0/8 (terminal blocked by tirith, parent-driven).
