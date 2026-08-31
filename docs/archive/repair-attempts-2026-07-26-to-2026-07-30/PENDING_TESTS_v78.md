# Pending Tests v78

- plan: docs/PENDING_PLAN_v78.md
- commit: docs/PENDING_COMMIT_v78.md
- approach: 3 Part A static fresh-probes via read_file + search_files; 0 runtime probes (terminal-blocked)

## Part A — static fresh spot-checks (cron-driven; NOT by-reference to v77 audit)
- T-A1: v3 spdlog `Pre-GIPass` HLVM_LOG at TestReSTIR_GI_Temporal.cpp:445 — PASS via read_file offset 440-455 (fresh this tick)
- T-A2: v3 spdlog `Post-GIPass` HLVM_LOG at TestReSTIR_GI_Temporal.cpp:452 — PASS via same read_file range
- T-A3: v3 spdlog `FGIPass::DispatchRays ENTER` HLVM_LOG at FGIPass.cpp:527 — PASS via search_files (fresh this tick; quintuple-print pattern with OutputTex/OutputW/OutputH/Frame/CmdList)
- T-A4: v3 spdlog `FGIPass::DispatchRays EXIT` HLVM_LOG at FGIPass.cpp:631 — PASS via same search_files (returns OutputTex only)

## Part B — runtime probes (parent-driven; tirith-blocked)
- T-B1 through T-B8: parent runs `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` + `cd Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` + validates with `validate_restir_gi.py` + vision-analyzes `display_frame8.png` + reports B8 zero-VUID check on stderr.log

## Probe target rationale
v3 spdlog markers are the diagnostic surface that the v12 cerr patch was designed to complement (spdlog=structured, cerr=bypass). If either ever drifts, parent loses the only signal that distinguishes spdlog-config-issue (log lines never appear) from source-binary-mismatch (log lines absent from binary). The 4 sites (TestReSTIR_GI_Temporal.cpp:445+452 + FGIPass.cpp:527+631) are well-known anchors; one fresh read_file covers all four.
