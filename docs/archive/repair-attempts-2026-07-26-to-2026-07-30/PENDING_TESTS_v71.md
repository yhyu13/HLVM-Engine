# Pending Tests v71

## Part A — Static probes (file-only, 9 fresh probes NOT by-reference to v70 audit)

| # | Probe | Site | Expected | Result |
|---|-------|------|----------|--------|
| A1 | v22 UAVBindingLayout split member | FGIPass.h:106 | present | PASS (1 hit) |
| A2 | v22 SRVBindingSet creation | FGIPass.cpp:570-571 | present | PASS |
| A3 | v22 UAVBindingSet creation | FGIPass.cpp:611-612 | present | PASS |
| A4 | v22 2-overload DispatchRays delegates | FRayTracingPipeline.cpp:344-345, 374-375, 381 | present | PASS |
| A5 | v41 std::clamp alpha encoder | FImageDump.cpp:27 | source alpha preserved | PASS |
| A6 | v38 cerr DebugMode effective= | FGIPass.cpp:487 | 4-field cerr | PASS |
| A7 | v28 alpha sentinel (Private master) | GIPathTracing.hlsl:694 | 0.99994f write | PASS |
| A8 | v28 alpha sentinel (data-dir copy) | TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl:694 | 0.99994f write | PASS |
| A9 | bug-088 executeCommandList | TestReSTIR_GI_Temporal.cpp:691 | present | PASS |

**Part A total: 9/9 PASS.**

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
9/9 Part A PASS; 0/8 Part B PENDING parent terminal. v71 structural-standby pattern is intact.
