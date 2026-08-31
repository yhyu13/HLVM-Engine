# Pending Tests v58
- tests: docs/PENDING_COMMIT_v58.md
- commit: docs/PENDING_COMMIT_v58.md
- tester: cron (single-head; file-only mode; per software-development-practices §TDD, file-only standby cycle)
- timestamp: 2026-07-28 (UTC)

## Part A — file-only verification of cumulative 21-patch inventory (FRESH probes, NOT by-reference to v57)

| # | Probe | Site | Expected | Result |
|---|-------|------|----------|--------|
| A1 | v22 split binding member | Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h:106 (UAVBindingLayout) | `nvrhi::BindingLayoutHandle UAVBindingLayout;` | PASS |
| A2 | v22 split binding init | Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:183 | `UAVBindingLayout = nullptr; // v22 split` | PASS |
| A3 | v22 split binding create | Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:311 | `UAVBindingLayout = Device->createBindingLayout(UAVLayoutDesc);` | PASS |
| A4 | v22 split binding use | Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:612 | `UAVBuilder.Build(), UAVBindingLayout);` | PASS |
| A5 | v22 2-overload DispatchRays | Engine/Source/Runtime/Private/Renderer/RayTracing/FRayTracingPipeline.cpp:381 | `DispatchRays(CmdList, Desc, SRVBindingSet, UAVBindingSet);` | PASS |
| A6 | v41 encoder alpha fix | Engine/Source/Runtime/Private/Image/FImageDump.cpp:27 | `pixels[idx + 3] = static_cast<uint8_t>(std::clamp(rgbaData[i * 4 + 3] * 255.0f, 0.0f, 255.0f));` | PASS |
| A7 | v38 cerr DebugMode effective | Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:487 | `std::cerr << "[RGI] FGIPass::WriteConstants: DebugMode effective=" << DebugMode` | PASS |
| A8 | v17 case 7u (BOTH HLSL copies) | Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl:604 + Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl:604 | `case 7u:  debugColor = diffuse * g_GI.AmbientColor.rgb * ambientScale; break;` | PASS |
| A9 | v28 alpha sentinel (BOTH HLSL copies) | GIPathTracing.hlsl:694 (both copies) | `Output[pixel].w = max(Output[pixel].w, 0.99994f);` | PASS |
| A10 | v37 validator alpha check | Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py:134 | `def check_alpha_sentinel(files, saturated_min=0.95, low_max=0.95):` | PASS |
| A11 | v40 dump_pixelstats alpha | Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py:96 | `def compute_alpha_stats(arr: np.ndarray) -> Optional[Tuple[float, float, int, float, float]]:` | PASS |
| A12 | bug-088 executeCommandList fix | Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp:691 | `executeCommandList(...)` intact | PASS |

12/12 PASS — all v22/v41/v38/v17/v28/v37/v40/bug-088 sites verified intact via fresh probes this tick.

## Part B — runtime tests (PARENT-DRIVEN, terminal blocked in cron)

| # | Test | How | Expected | Status |
|---|------|-----|----------|--------|
| B1 | Build cleanliness | `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` | exit 0, no -Werror cascade failures | PENDING (parent-driven) |
| B2 | Default run with HLSL sentinels + alpha sentinel +38 cerr writes + encoder alpha fix | `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` | 8 `[RGI] Render() entry:` + 8 `[RGI] FGIPass::DispatchRays() entry:` + 8 `DebugMode effective=` lines | PENDING |
| B3 | Dump inspection (alpha channel) | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py dumps/LATEST` | v40-alpha verdict line: `saturated` (dispatch ran) / `zero` (didn't run) / `mixed` / `low` / `unknown` | PENDING |
| B4 | Validator (with v37 alpha check) | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` | PASS 3/3 OR 4/4 (alpha included) | PENDING |
| B5 | Vision analysis | vision_analyze on `dumps/LATEST/display_frame8.png` | recognizable non-uniform Sponza geometry | PENDING |
| B6 | Vulkan VUID check | `grep stderr.log VUID-VkDescriptorImageInfo-imageLayout-00344` | 0 matches (v22 fix eliminates the binding-ordering warning) | PENDING |
| B7 | Mode 6 evidence | re-run with `HLVM_PT_DEBUG_MODE=6`; dump gi_raw | per-pixel gradient (mode 6 dispatches + UAV write lands) | PENDING |
| B8 | decode_v38_evidence | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/decode_v38_evidence.py --cerr-file stderr.log` | structured verdict (GO / FIX_ATOI / FIX_DOCS / FIX_CVAR / NO_CERR / MIXED / UNRECOGNIZED) | PENDING |

8/8 PENDING — terminal-blocked in cron; parent drives B1-B8.
