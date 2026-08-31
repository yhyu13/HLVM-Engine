# Pending Tests v76

- plan: docs/PENDING_PLAN_v76.md
- commit: docs/PENDING_COMMIT_v76.md
- approach: 2 Part A static spot-checks via read_file + search_files; 0 runtime probes (terminal-blocked)

## Part A — static spot-checks (cron-driven)
- T-A1: v54 doc-drift cross-reference "line 1531" at TestReSTIR_GI_Temporal.cpp:407 — PASS via read_file offset 400-421
- T-A2: v54 doc-drift cross-reference "line 1531" at TestReSTIR_GI_Temporal.cpp:676 — PASS via read_file offset 670-684
- T-A3: search_files "line 1531" cross-reference count = 2 (matches expected at :407+676) — PASS

## Part B — runtime probes (parent-driven; tirith-blocked)
- T-B1 through T-B8: parent runs `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` + `cd Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` + validates with `validate_restir_gi.py` + vision-analyzes `display_frame8.png` + reports B8 zero-VUID check on stderr.log

## Spot-check: v41 alpha-encoder
- S-A1: std::clamp(rgbaData[i*4+3] * 255.0f, 0.0f, 255.0f) at Private/Image/FImageDump.cpp:27 — PASS via search_files (1 hit)
