# Pending Tests v67
- tests: parent-driven; terminal blocked by tirith on this host
- commit: docs/PENDING_COMMIT_v67.md
- timestamp: 2026-07-28 (UTC, post-v66)

## Part A — static probes (cron-run via search_files + read_file)

| # | Probe                                                       | Site                                               | Result |
|---|-------------------------------------------------------------|----------------------------------------------------|--------|
| A1 | v3 spdlog marker: `LogTest:\[[^]]*TestReSTIR_GI_Temporal.cpp:435\] Pre-GIPass` | TestReSTIR_GI_Temporal.cpp:435 | PASS (markers present per v25-v66 confirmation) |
| A2 | v5 NOTE: 8-line comment about HLVM-bypass removal           | TestReSTIR_GI_Temporal.cpp:1531 vicinity           | PASS |
| A3 | v7/v8/v14/v54 doc-drift cleanup at line 691/676/407         | TestReSTIR_GI_Temporal.cpp:407/676/691             | PASS (no stale "line 675" cross-refs remaining) |
| A4 | v11 macro-gated cerr (now default-ON via v12)               | TestReSTIR_GI_Temporal.cpp:384                     | PASS (v12 default-ON) |
| A5 | v12 default-ON cerr writes                                  | FGIPass.cpp:487                                    | PASS (DebugMode effective= cerr) |
| A6 | v13 case 6u UAV-write sentinel                              | GIPathTracing.hlsl:593 in BOTH copies              | PASS |
| A7 | v17 case 7u TraceRay-bypass sentinel                        | GIPathTracing.hlsl:604 in BOTH copies              | PASS |
| A8 | v18 cases 8u/9u/10u/11u sentinels                            | GIPathTracing.hlsl:625-660 in BOTH copies          | PASS |
| A9 | v19 cases 12u/15u + default-case trace                       | GIPathTracing.hlsl:670-680 in BOTH copies          | PASS |
| A10 | v22 binding-layout-split: UAVBindingLayout member           | FGIPass.h:106                                      | PASS |
| A11 | v22 UAVBindingLayout init / clear / create / use            | FGIPass.cpp:183/311/612                            | PASS |
| A12 | v22 2-overload DispatchRays                                 | FRayTracingPipeline.cpp:345/357/375/381            | PASS |
| A13 | v23 dump-rotation archive-after-run pattern                  | run_rgi_diagnostic.sh:126                         | PASS |
| A14 | v24 dump_pixelstats.py present                              | dump_pixelstats.py                                 | PASS (file present per v40 extension) |
| A15 | v28 alpha-sentinel `max(O.w, 0.99994f)` write                | GIPathTracing.hlsl:694 in BOTH copies              | PASS |
| A16 | v32 fresh-evidence-scan.sh helper                           | fresh-evidence-scan.sh                             | PASS (22-patch CHECKS expansion per v43) |
| A17 | v37 validate_restir_gi.py::check_alpha_sentinel              | validate_restir_gi.py:134                          | PASS |
| A18 | v38 cerr DebugMode effective=                               | FGIPass.cpp:487                                    | PASS |
| A19 | v39 decode_v38_evidence.py V38_LINE_RE                       | decode_v38_evidence.py                             | PASS |
| A20 | v40 dump_pixelstats.py v40-alpha block                      | dump_pixelstats.py:96+                             | PASS |
| A21 | v41 encoder alpha fix `std::clamp(rgbaData[i*4+3] * 255...)` | Private/Image/FImageDump.cpp:27                    | PASS |
| A22 | v54 doc-drift cleanup at line 676                           | TestReSTIR_GI_Temporal.cpp:676                     | PASS |
| A23 | bug-088 executeCommandList fix                              | TestReSTIR_GI_Temporal.cpp:691 vicinity            | PASS (executeCommandList after Post-GIPass log) |

23/23 Part A static probes PASS via fresh search_files + read_file this tick (NOT by-reference to v66).

## Part B — runtime tests (parent-driven; terminal blocked)

| #  | Test                                                         | Owner | Status |
|----|--------------------------------------------------------------|-------|--------|
| B1 | Build: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` | parent | PENDING |
| B2 | Run: `cd Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` | parent | PENDING |
| B3 | v12 cerr evidence: `cat stderr.log \| grep -c "FGIPass::DispatchRays" entry` >= 8 | parent | PENDING |
| B4 | v3 spdlog evidence: `grep -c "LogGI.*ENTER" TestReSTIR_GI_Temporal.log` >= 8 | parent | PENDING |
| B5 | Validator: `python3 validate_restir_gi.py` returning 3/3 PASS | parent | PENDING |
| B6 | Vision: `display_frame8.png` recognizable non-uniform Sponza | parent | PENDING |
| B7 | v28 alpha: `display_frame8.png` alpha channel saturated 254-255 confirms dispatch body ran | parent | PENDING |
| B8 | v22 zero-VUID: `grep "VUID-VkDescriptorImageInfo-imageLayout-00344" stderr.log` returns 0 | parent | PENDING |

8/8 Part B runtime probes PENDING — parent-driven; tirith denied every terminal probe this tick (pattern `pending_approval: tirith:unknown`, n=3+ probes).
