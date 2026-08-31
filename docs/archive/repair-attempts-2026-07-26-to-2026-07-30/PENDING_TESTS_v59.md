# Pending Tests v59

## Part A — file-only static verification (cron-driven, this tick)

| Test | Probe | Result |
|------|-------|--------|
| A1 | search_files FGIPass.h:106 UAVBindingLayout member (v22 split) | PASS |
| A2 | search_files FGIPass.cpp:183 UAVBindingLayout init/clear (v22) | PASS |
| A3 | search_files FGIPass.cpp:311/612 UAVBindingLayout createBindingLayout/use (v22) | PASS |
| A4 | search_files FRayTracingPipeline.cpp:381 2-overload DispatchRays (v22) | PASS |
| A5 | read_file FImageDump.cpp:16-18 std::clamp alpha pattern (v41 encoder fix) | PASS |
| A6 | read_file FGIPass.cpp:487 cerr DebugMode effective= (v38) | PASS |
| A7 | search_files GIPathTracing.hlsl:604 case 7u BOTH copies (v17) | PASS |
| A8 | search_files GIPathTracing.hlsl:694 Output[pixel].w sentinel BOTH copies (v28) | PASS |
| A9 | search_files TestReSTIR_GI_Temporal.cpp:691 executeCommandList (bug-088) | PASS |

**Part A: 9/9 PASS.**

## Part B — runtime verification (terminal-blocked, parent-driven)

| Test | Probe | Status |
|------|-------|--------|
| B1 | Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal builds cleanly | PENDING (tirith) |
| B2 | ./TestReSTIR_GI_Temporal with HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 runs without crash | PENDING |
| B3 | stderr.log contains v12/v38 default-ON cerr lines per frame | PENDING |
| B4 | TestReSTIR_GI_Temporal.log contains 0 "command list should be executed" warnings | PENDING |
| B5 | Validator: python3 validate_restir_gi.py on newest dump group — 3/3 PASS | PENDING |
| B6 | Vision: display_frame8.png has recognizable non-uniform Sponza geometry | PENDING |
| B7 | VUID: grep stderr.log VUID-VkDescriptorImageInfo-imageLayout-00344 returns 0 | PENDING |
| B8 | Alpha-channel inspection: dump_pixelstats.py [v40-alpha] reports saturated 254-255 (v28 sentinel landed) | PENDING |

**Part B: 8/8 PENDING (terminal-blocked; parent-action recipe in PIPELINE_HEALTH parent-triage section).**
