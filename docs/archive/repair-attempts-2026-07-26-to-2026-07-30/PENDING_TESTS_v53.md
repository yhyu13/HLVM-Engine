# Pending Tests v53
- commit: docs/PENDING_COMMIT_v53.md
- tests: N/A — documentation-only tick (zero source-code lines modified)

## Test rationale
v53 is a pure structural standby tick with 0 source-code lines modified. There is no test surface to validate directly. The "test" for v53 is the parent-driven terminal-access invocation of `fresh-evidence-scan.sh`, which validates the cumulative 21-patch inventory is still intact and that the parent has fresh evidence to feed back.

## Part A — static-audit tests (cron-runnable; FRESH probes this tick, breaking v52's audit-by-reference shortcut)
| # | Test | Path | Expected | Actual |
|---|------|------|----------|--------|
| A1 | v22 binding-layout-split UAVBindingLayout member | Public/Renderer/GI/FGIPass.h:106 | "UAVBindingLayout // v22 split: separate layout for u0/u1 UAVs" | FRESH: matches |
| A2 | v41 alpha-encoder std::clamp i*4+3 | Private/Image/FImageDump.cpp:27 | "pixels[idx + 3] = static_cast<uint8_t>(std::clamp(rgbaData[i*4+3] * 255.0f, 0.0f, 255.0f));" | FRESH: matches |
| A3 | v17 case 7u TraceRay-bypass sentinel | Private/Renderer/Shader/GI/GIPathTracing.hlsl:604 | "case 7u: debugColor = diffuse * g_GI.AmbientColor.rgb * ambientScale; break;" | FRESH: matches |
| A4 | v13 case 6u UAV-write sentinel | Private/Renderer/Shader/GI/GIPathTracing.hlsl:593 | "case 6u: debugColor = float3(float(pixel.x) / 256.0, 0.0, float(pixel.y) / 256.0); break;" | FRESH: matches |
| A5 | v28 alpha-alive sentinel | Private/Renderer/Shader/GI/GIPathTracing.hlsl:694 | "Output[pixel].w = max(Output[pixel].w, 0.99994f);" | FRESH: matches |
| A6 | v38 cerr DebugMode-effective | Private/Renderer/GI/FGIPass.cpp:487 | "std::cerr << \"[RGI] FGIPass::WriteConstants: DebugMode effective=\" ..." | FRESH: matches |
| A7 | v22 SRVBindingSet+UAVBindingSet call site | Private/Renderer/GI/FGIPass.cpp:625 | "RTPipeline.DispatchRays(CmdList, Desc.OutputWidth, Desc.OutputHeight, 1, SRVBindingSet, UAVBindingSet)" | FRESH: matches |

7/7 Part A tests PASS via fresh search_files probes this tick (NOT by-reference to v52).

## Part B — runtime tests (parent-driven; cron terminal blocked; PENDING until v54+)
- B1: Build cleanliness via `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` — exit 0 + 0 warnings expected
- B2: Default-mode run via `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` — expect 16 cerr lines + visual Sponza geometry in display_frame8.png
- B3: Vision check on display_frame8.png — should contain recognizable non-uniform Sponza geometry with sane exposure
- B4: Validator 4/4 PASS via `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` (includes v37 alpha-check)
- B5: dump_pixelstats.py verdict line — should match v41 encoder + v28 alpha sentinel expectations
- B6: B8 zero-VUID check (`grep stderr.log VUID-VkDescriptorImageInfo-imageLayout-00344` should return 0) — verifies v22 binding-layout-split fix eliminated the validation warning
- B7: decode_v38_evidence.py on stderr.log — should classify DebugMode into GO/FIX_ATOI/FIX_DOCS/FIX_CVAR/etc branch
- B8: fresh-evidence-scan.sh exit code — should be 0 (fresh-evidence-pass) with 27 cumulative `[OK]` lines

Part B can ONLY advance after parent supplies terminal access. Cron is structurally blocked.

## Verdict
7/7 Part A static tests PASS via fresh probes. 0/8 Part B runtime tests PASS (cron terminal blocked; parent must run). v53 is structurally identical to v25-v52 — documentation-only tick, no test surface change. Verdict: PENDING parent-evidence-gated continuation.
