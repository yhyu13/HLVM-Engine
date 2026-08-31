# Pending Tests v72

## Part A — Static probes (file-only, 10 fresh probes NOT by-reference to v71 audit)

| # | Probe | Site | Expected | Result |
|---|-------|------|----------|--------|
| A1 | v22 UAVBindingLayout split member | Public/Renderer/GI/FGIPass.h:106 | present | PASS (1 hit) |
| A2 | v22 UAVBindingLayout init/clear | Private/Renderer/GI/FGIPass.cpp:183 | present | PASS |
| A3 | v22 SRV/UAV split doc comments | FGIPass.cpp:281-282, 296 | u0/u1 moved to UAVBindingLayout | PASS |
| A4 | v22 UAVBindingLayout create | FGIPass.cpp:311-312 | present | PASS |
| A5 | v22 UAVBuilder.Build() use | FGIPass.cpp:612 | present | PASS |
| A6 | v22 SRVBindingSet+UAVBindingSet overload | Public/Renderer/RayTracing/FRayTracingPipeline.h:188, :194 | present | PASS |
| A7 | v22 2-binding dispatch forward | Private/Renderer/RayTracing/FRayTracingPipeline.cpp:381 | DispatchRays(CmdList, Desc, SRVBindingSet, UAVBindingSet) | PASS |
| A8 | v41 std::clamp alpha encoder | Private/Image/FImageDump.cpp:16-18, :27 | source alpha preserved | PASS |
| A9 | v38 cerr DebugMode effective= | Private/Renderer/GI/FGIPass.cpp:487 | present | PASS |
| A10 | v28 alpha sentinel + 0.99994f write in BOTH HLSL copies | Private/Renderer/Shader/GI/GIPathTracing.hlsl:689/:694 + TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl:689/:694 | Output[pixel].w = max(..., 0.99994f) | PASS |

**Part A total: 10/10 PASS.**

## Part B — Runtime probes (terminal-blocked; parent-driven)

| # | Probe | Site | Expected | Result |
|---|-------|------|----------|--------|
| B1 | Build cleanliness | `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` | exit 0 | PENDING parent terminal |
| B2 | Fresh dump | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal` | 8 frames written | PENDING parent terminal |
| B3 | Stderr closure | cat stderr.log | 8 `[RGI]` cerr lines | PENDING parent terminal |
| B4 | v3 spdlog markers | grep TestReSTIR_GI_Temporal.log | per-frame spdlog lines | PENDING parent terminal |
| B5 | Mode 6/7 sentinels | HLVM_PT_DEBUG_MODE=6/7 + dump_pixelstats.py | per-pixel gradient + scene shape | PENDING parent terminal |
| B6 | Validator | `python3 validate_restir_gi.py` | 3/3 PASS (+ optional alpha-check from v37) | PENDING parent terminal |
| B7 | Vision analysis | open display_frame8.png | recognizable non-uniform Sponza geometry | PENDING parent terminal |
| B8 | v22 zero-VUID check | `grep stderr.log VUID-VkDescriptorImageInfo-imageLayout-00344` | 0 matches | PENDING parent terminal |

**Part B total: 0/8 (terminal blocked by tirith).**

## Verdict
10/10 Part A PASS; 0/8 Part B PENDING parent terminal. v72 structural-standby pattern is intact.
