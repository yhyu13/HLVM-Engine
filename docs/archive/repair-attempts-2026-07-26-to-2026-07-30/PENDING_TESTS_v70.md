# Pending Tests v70
- tests: docs/PENDING_TESTS_v70.md
- commit: docs/PENDING_COMMIT_v70.md

## Part A — Static probes (file-only, cron-executable)

| ID | Patch | Site | Expected | This tick |
|----|-------|------|----------|-----------|
| A1 | v22 UAVBindingLayout member | Public/Renderer/GI/FGIPass.h:106 | `nvrhi::BindingLayoutHandle UAVBindingLayout;` | PASS |
| A2 | v22 split init at clear | Private/Renderer/GI/FGIPass.cpp:183 | `UAVBindingLayout       = nullptr; // v22 split: clear separate UAV layout` | PASS |
| A3 | v22 split createBindingLayout | Private/Renderer/GI/FGIPass.cpp:311 | `UAVBindingLayout = Device->createBindingLayout(UAVLayoutDesc);` | PASS |
| A4 | v22 UAVBindingSet use | Private/Renderer/GI/FGIPass.cpp:612 | `UAVBuilder.Build(), UAVBindingLayout);` | PASS |
| A5 | v22 SRVBindingSet 2-overload DispatchRays | Private/Renderer/RayTracing/FRayTracingPipeline.cpp:345/357/375/381 | 4 sites intact | PASS |
| A6 | v41 std::clamp alpha-encoder | Private/Image/FImageDump.cpp:27 | `pixels[idx + 3] = static_cast<uint8_t>(std::clamp(rgbaData[i * 4 + 3] * 255.0f, 0.0f, 255.0f));` | PASS |
| A7 | v38 cerr DebugMode effective= | Private/Renderer/GI/FGIPass.cpp:487 | `std::cerr << "[RGI] FGIPass::WriteConstants: DebugMode effective=" << DebugMode` | PASS |
| A8 | v13 case 6u UAV-write sentinel | BOTH GIPathTracing.hlsl copies at :593 | `case 6u:  debugColor = float3(float(pixel.x) / 256.0, 0.0, float(pixel.y) / 256.0); break;` | PASS |
| A9 | v17 case 7u TraceRay-bypass sentinel | BOTH GIPathTracing.hlsl copies at :604 | `case 7u:  debugColor = diffuse * g_GI.AmbientColor.rgb * ambientScale; break;` | PASS |
| A10 | v28 alpha-channel alive-sentinel | BOTH GIPathTracing.hlsl copies at :694 | `Output[pixel].w = max(Output[pixel].w, 0.99994f);` | PASS |
| A11 | bug-088 executeCommandList fix | TestReSTIR_GI_Temporal.cpp:691 | `NvrhiDevice->executeCommandList(CommandList);` | PASS |

**Summary: 11/11 Part A static probes PASS.** Cumulative 22-patch inventory verified intact via fresh search_files probes (NOT by-reference to v69 audit).

## Part B — Runtime probes (PENDING, parent-driven, terminal blocked by tirith)

| ID | Test | Command | Expected | Status |
|----|------|---------|----------|--------|
| B1 | Build cleanliness | `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` | 0 errors, 0 new warnings, target links | PENDING (terminal blocked) |
| B2 | Default-mode run + stderr capture | `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` | stderr.log contains 16 cerr lines from v12+v38 | PENDING |
| B3 | stderr VUID-00344 check | `grep -c VUID-VkDescriptorImageInfo-imageLayout-00344 stderr.log` | 0 hits (v22 binding-layout-split goal) | PENDING |
| B4 | Alpha-channel dump-pixelstats inspection | `python3 .../dump_pixelstats.py display_frame8.png` | alpha channel saturated 254-255 (v28 + v41 pipeline works) | PENDING |
| B5 | Mode 6 UAV-write sentinel | `HLVM_PT_DEBUG_MODE=6 ./TestReSTIR_GI_Temporal` | per-pixel gradient | PENDING |
| B6 | Mode 7 TraceRay-bypass | `HLVM_PT_DEBUG_MODE=7 ./TestReSTIR_GI_Temporal` | scene-shape × 1.5 | PENDING |
| B7 | Validator verdict | `python3 .../validate_restir_gi.py` | 4/4 PASS | PENDING |
| B8 | Decoder verdict | `python3 .../decode_v38_evidence.py --cerr-file stderr.log` | structured verdict | PENDING |
| B9 | Vision analysis | open `display_frame8.png` via vision_analyze | recognizable non-uniform Sponza geometry | PENDING |

## Per-test verdict
Part A: 11/11 PASS. Part B: 0/9 (terminal blocked by tirith, 36th consecutive cycle).

## Cumulative inventory verified this turn
All 22 patches intact: v3/v5/v7/v8/v11/v12/v13/v14/v15/v17/v18/v19/v22/v23/v24/v28/v37/v38/v39/v40/v41/v54 + bug-088 + bug-075.
