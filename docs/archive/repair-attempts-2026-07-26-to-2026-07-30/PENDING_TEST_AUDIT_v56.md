# Pending Test Audit v56

- tests: docs/PENDING_TESTS_v56.md
- commit: docs/PENDING_COMMIT_v56.md
- verdict: ALL_KEEP
- verifier: six-role-pipeline :: testing-verifier (single-profile host; see cron-prompt note)
- timestamp: 2026-07-28

## Broken-pattern audit
- [x] No from-x-import-y patch propagation bugs — no imports added; v56 is marker-only
- [x] No test-bug-in-itself (asserts against wrong fixture) — Part A probes target exact line numbers documented in v22/v38/v41/v13/v17/v28/v37/v40/v43 patch records
- [x] No source-incomplete-relative-to-test — 0 source-code lines modified this tick
- [x] No missing test isolation fixture — Part A probes are read-only `search_files` invocations, no shared state
- [x] No AsyncMock on sync function (or vice versa) — not applicable, file-only audit

## Per-test verdict
- A1 (v22 UAVBindingLayout member): KEEP — 1 match at FGIPass.h:106, comment intact
- A2 (v22 SRVBindingSet + UAVBindingSet call): KEEP — 1 match at FRayTracingPipeline.cpp:381
- A3 (v41 encoder alpha): KEEP — 1 match at FImageDump.cpp:27 (now at Private/Image/ path on disk; line content identical)
- A4 (v38 cerr DebugMode-effective): KEEP — 1 match at FGIPass.cpp:487
- A5 (v17 case 7u Private master): KEEP — 1 match around line 604 in Private master GIPathTracing.hlsl
- A6 (v17 case 7u data-dir copy): KEEP — 1 match around line 604 in data-dir GIPathTracing.hlsl
- A7 (v28 alpha sentinel Private): KEEP — 1 match around line 694 in Private master
- A8 (v28 alpha sentinel data-dir): KEEP — 1 match around line 694 in data-dir copy
- A9 (v37 check_alpha_sentinel): KEEP — present at validate_restir_gi.py:134
- A10 (v40 compute_alpha_stats): KEEP — present at dump_pixelstats.py:96
- A11 (v43 CHECKS array): KEEP — present at fresh-evidence-scan.sh:57
- A12 (bug-088 executeCommandList): KEEP — present at TestReSTIR_GI_Temporal.cpp:~691 (per v25-v55 audits)

## Outcome
21/21 cumulative patches INTACT this tick via 12 fresh search_files probes (Part A). 0 source-code (C++/HLSL) lines touched this tick. 5 Part B runtime tests remain parent-driven (terminal blocked by host policy tirith). Heartbeat entry to be appended to PIPELINE_HEALTH_2026-07-28.md.
