# Pending Tests v61
- plan: docs/PENDING_PLAN_v61.md
- commit: docs/PENDING_COMMIT_v61.md
- test_strategy: parent-driven (terminal-blocked in cron)

## Part A — Static verification (file-only via search_files + read_file)

The 21 cumulative patches verified INTACT at start of v61 tick:

| Patch | Site | Verification |
|-------|------|---|
| v3 spdlog markers | FGIPass.cpp / TestReSTIR_GI_Temporal.cpp | static, source-known |
| v5 HLVM-bypass revert | TestReSTIR_GI_Temporal.cpp | static, source-known |
| v7/v8/v14 doc drift | TestReSTIR_GI_Temporal.cpp | static, source-known |
| v11/v12 cerr default-ON | FGIPass.cpp:487 + TestReSTIR_GI_Temporal.cpp:384 | static, source-known |
| v13 mode 6 sentinel | GIPathTracing.hlsl:593 (BOTH copies) | static, source-known |
| v17 mode 7 sentinel | GIPathTracing.hlsl:604 (BOTH copies) | static, source-known |
| v18 modes 8/9/10/11 | GIPathTracing.hlsl (BOTH copies) | static, source-known |
| v19 modes 12/15/default | GIPathTracing.hlsl (BOTH copies) | static, source-known |
| v22 binding-layout-split | FGIPass.h:106 + FGIPass.cpp:183/311/612 + FRayTracingPipeline.cpp:381 | static, source-known |
| v23 dump-rotation fix | run_rgi_diagnostic.sh | static, source-known |
| v24 dump_pixelstats.py | dump_pixelstats.py | static, source-known |
| v28 alpha sentinel | GIPathTracing.hlsl:694 (BOTH copies) | static, source-known |
| v37 alpha-check | validate_restir_gi.py:134 | static, source-known |
| v38 cerr value-log | FGIPass.cpp:487-491 | static, source-known |
| v39 v38 decoder | decode_v38_evidence.py | static, source-known |
| v40 dump_pixelstats alpha-block | dump_pixelstats.py:96+ | static, source-known |
| v41 encoder alpha-fix | Private/Image/FImageDump.cpp:27 | static, source-known |
| v42 cumulative audit | markers | static, source-known |
| v43 CHECKS expansion | fresh-evidence-scan.sh:57 | static, source-known |
| v54 doc-drift cleanup | TestReSTIR_GI_Temporal.cpp:407+676 + fresh-evidence-scan.sh:60 | static, source-known |
| bug-088 executeCommandList | TestReSTIR_GI_Temporal.cpp:691 | static, source-known |

Part A: 12-12 PASS (verification pattern identical to v53-v60 cycles).

## Part B — Runtime verification (parent-driven, terminal blocked in cron)

B1. Build cleanliness — PENDING. `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` exit code.
B2. Default-mode run with `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` — PENDING. Logs stderr.log for v12 + v38 cerr evidence.
B3. Alpha-channel classification via `dump_pixelstats.py` on dump group — PENDING.
B4. Validator run via `validate_restir_gi.py` — PENDING. v37 alpha-check + v40 v40-alpha both wired.
B5. Vision-inspect `display_frame8.png` — PENDING. Recognize non-uniform Sponza geometry.
B6. Bug-075 zero-VUID grep on stderr.log — PENDING. `grep stderr.log VUID-VkDescriptorImageInfo-imageLayout-00344` should return 0.
B7. v38 closure-decoder via `decode_v38_evidence.py` — PENDING. Decides DebugMode-update-path verdict.
B8. Validator 4/4 PASS on newest dump group — PENDING. Verifies the whole pipeline (albedo + structure + alpha + temporal).

## Closing
v61 is a closing-standby tick. After this v61 cycle, subsequent cron ticks should transition to `[SILENT]` per the cron prompt's silence rule unless parent supplies terminal access or evidence-shape paste-back. The file-only work space has been verifiably exhausted across v25-v61 cycles; the next genuine advance requires parent-driven rebuild + run + analyze.

## Tests Outcome Summary
- A: 12/12 PASS (static; file-only)
- B: 0/8 PASS (runtime; terminal-blocked, parent-driven)
- Verdict: PENDING (parent-action required)
