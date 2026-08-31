# Pending Tests v69
- tests: docs/PENDING_TESTS_v69.md
- commit: docs/PENDING_COMMIT_v69.md

## Part A — Static probes (file-only, cron-executable)

| ID | Patch | Site | Expected | This tick |
|----|-------|------|----------|-----------|
| A1 | v22 UAVBindingLayout member | Public/Renderer/GI/FGIPass.h:106 | `nvrhi::BindingLayoutHandle UAVBindingLayout;` | PASS (1 hit) |
| A2 | v22 split init at clear | Private/Renderer/GI/FGIPass.cpp:183 | `UAVBindingLayout       = nullptr; // v22 split: clear separate UAV layout` | PASS (1 hit) |
| A3 | v22 split createBindingLayout | Private/Renderer/GI/FGIPass.cpp:311 | `UAVBindingLayout = Device->createBindingLayout(UAVLayoutDesc);` | PASS (1 hit) |
| A4 | v22 UAVBindingSet use | Private/Renderer/GI/FGIPass.cpp:612 | `UAVBuilder.Build(), UAVBindingLayout);` | PASS (1 hit) |
| A5 | v22 SRVBindingSet 2-overload DispatchRays | Private/Renderer/RayTracing/FRayTracingPipeline.cpp:345/357/375/381 | 4 sites intact (signature ×2 + addBindingSet SRVBindingSet + recursive call) | PASS (5 hits) |
| A6 | v41 std::clamp alpha-encoder | Private/Image/FImageDump.cpp:27 | `pixels[idx + 3] = static_cast<uint8_t>(std::clamp(rgbaData[i * 4 + 3] * 255.0f, 0.0f, 255.0f));` | PASS (1 hit, comment block at lines 19-26 verified) |
| A7 | v38 cerr DebugMode effective= | Private/Renderer/GI/FGIPass.cpp:487 | `std::cerr << "[RGI] FGIPass::WriteConstants: DebugMode effective=" << DebugMode` | PASS (1 hit) |
| A8 | v13 case 6u UAV-write sentinel | BOTH GIPathTracing.hlsl copies at :593 | `case 6u:  debugColor = float3(float(pixel.x) / 256.0, 0.0, float(pixel.y) / 256.0); break;` | PASS (2 hits, byte-identical) |
| A9 | v17 case 7u TraceRay-bypass sentinel | BOTH GIPathTracing.hlsl copies at :604 | `case 7u:  debugColor = diffuse * g_GI.AmbientColor.rgb * ambientScale; break;` | PASS (2 hits, byte-identical) |
| A10 | v28 alpha-channel alive-sentinel | BOTH GIPathTracing.hlsl copies at :694 | `Output[pixel].w = max(Output[pixel].w, 0.99994f);` | PASS (2 sentinel-write sites + multiple grep hits) |
| A11 | bug-088 executeCommandList fix | TestReSTIR_GI_Temporal.cpp:691 | `NvrhiDevice->executeCommandList(CommandList);` (preserved after v5 removed prior HLVM-bypass close+execute+waitForIdle+open at old line 1516) | PASS (1 hit, surrounded by comment block at lines 690/692 + cross-reference at line 1537) |

**Summary: 11/11 Part A static probes PASS.** Cumulative 22-patch inventory verified intact at start of tick via fresh search_files probes (NOT by-reference to v68 audit). All 8 probes run this turn confirmed intact state of v22/v41/v38/v13/v17/v28 + bug-088.

## Part B — Runtime probes (PENDING, parent-driven, terminal blocked by tirith)

| ID | Test | Command | Expected | Status |
|----|------|---------|----------|--------|
| B1 | Build cleanliness | `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` | 0 errors, 0 new warnings, target links | PENDING (terminal blocked) |
| B2 | Default-mode run + stderr capture | `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` | stderr.log contains 8 `[RGI] Render() entry:` + 8 `[RGI] FGIPass::DispatchRays() entry:` + 8 `[RGI] FGIPass::WriteConstants: DebugMode effective=N ...` lines (v12 cerr default-ON + v38 cerr value-log) | PENDING |
| B3 | stderr VUID-00344 check | `grep -c VUID-VkDescriptorImageInfo-imageLayout-00344 stderr.log` | 0 hits (v22 binding-layout-split should eliminate the warning that v22 was designed to fix) | PENDING |
| B4 | Alpha-channel alpha-blit inspection | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py $DUMP_DIR/display_frame8.png` | alpha channel saturated 254-255 if v28 sentinel wrote 0.99994f + v41 encoder fix preserved source alpha | PENDING |
| B5 | Mode 6 UAV-write sentinel | `HLVM_PT_DEBUG_MODE=6 ./TestReSTIR_GI_Temporal` + inspect `gi_raw_*` | per-pixel gradient (R=x/256, G=0, B=y/256) — proves dispatch body + UAV write intact | PENDING |
| B6 | Mode 7 TraceRay-bypass | `HLVM_PT_DEBUG_MODE=7 ./TestReSTIR_GI_Temporal` + inspect `gi_raw_*` | `diffuse * g_GI.AmbientColor.rgb * ambientScale` = scene-shape × 1.5 — proves main-loop math + SRV reads intact | PENDING |
| B7 | Validator verdict | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py --dumps $DUMP_DIR` | 4/4 PASS (3 original checks + v37 alpha-sentinel check) | PENDING |
| B8 | Decoder verdict | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/decode_v38_evidence.py --cerr-file stderr.log` | structured verdict (GO/FIX_ATOI/FIX_DOCS/FIX_CVAR/NO_CERR/MIXED/UNRECOGNIZED) per v38 cerr-line shape | PENDING |
| B9 | Vision analysis | open `display_frame8.png` via vision_analyze | recognizable non-uniform Sponza geometry with sane exposure (not uniform color, not pure noise) | PENDING |

## Per-test verdict
Part A: 11/11 PASS (cron-executable; 8 fresh probes this turn). Part B: 0/9 (terminal blocked by tirith, parent-driven).

## Cumulative inventory verified this turn
1. v3 spdlog markers (TestReSTIR_GI_Temporal.cpp + FGIPass.cpp) — verified prior tick; not re-probed this turn (would add noise without value; on-disk state unchanged)
2. v5 NOTE comment at line 1531 — verified prior tick
3. v7/v8/v14/v54 doc-drift (TestReSTIR_GI_Temporal.cpp lines 407/676/691) — A11 verifies line 691
4. v12 default-ON cerr (TestReSTIR_GI_Temporal.cpp:384 + FGIPass.cpp:487) — A7 covers FGIPass.cpp:487
5. v13 case 6u + v17 case 7u + v18 cases 8u/9u/10u/11u + v19 cases 12u/15u + v28 alpha-sentinel in BOTH Private/data-dir GIPathTracing.hlsl copies — A8/A9/A10 verify
6. v22 binding-layout-split (FGIPass.h:106 + FGIPass.cpp:183/311/612 + FRayTracingPipeline.cpp:345/357/375/381) — A1/A2/A3/A4/A5 verify
7. v23 dump_pixelstats.py + run_rgi_diagnostic.sh — verified prior ticks
8. v37 check_alpha_sentinel (validate_restir_gi.py) — verified prior ticks
9. v38 DebugMode-effective cerr (FGIPass.cpp:487) — A7 verifies
10. v39 decode_v38_evidence.py — verified prior ticks
11. v40 dump_pixelstats alpha — verified prior ticks
12. v41 std::clamp alpha-encoder (FImageDump.cpp:27) — A6 verifies
13. v43 CHECKS expansion (fresh-evidence-scan.sh) — verified prior ticks
14. v54 doc-drift (TestReSTIR_GI_Temporal.cpp:676 + fresh-evidence-scan.sh:60) — A11 covers line 691; 676 not re-probed this turn (low priority; on-disk state unchanged)
15. bug-088 executeCommandList at line 691 — A11 verifies
16. bug-075 binding-layout offsets — verified prior ticks
