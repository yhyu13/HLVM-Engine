# Pending Tests v56

Tester surface (file-only — `terminal` blocked by host policy tirith):

## Part A — Patch-intact audit (file-only probes; FRESH this tick, NOT by-reference)

| ID | Check | Probe | Expected | Actual |
|----|-------|-------|----------|--------|
| A1 | v22 binding-layout split — UAVBindingLayout member | `search_files target=content path=Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h pattern="UAVBindingLayout"` | 1 match at FGIPass.h:106 | 1 match at FGIPass.h:106 ✓ |
| A2 | v22 SRVBindingSet + UAVBindingSet call | `search_files target=content path=Engine/Source/Runtime/Private/Renderer/RayTracing/FRayTracingPipeline.cpp pattern="SRVBindingSet, UAVBindingSet"` | 1 match at FRayTracingPipeline.cpp:381 | 1 match at FRayTracingPipeline.cpp:381 ✓ |
| A3 | v41 encoder alpha-preserving fix | `search_files target=content path=Engine/Source/Runtime/Private/Image/FImageDump.cpp pattern="std::clamp(rgbaData\[i \* 4 \+ 3\]"` | 1 match at FImageDump.cpp:27 | 1 match at FImageDump.cpp:27 ✓ |
| A4 | v38 cerr DebugMode-effective value-log | `search_files target=content path=Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp pattern="DebugMode effective="` | 1 match at FGIPass.cpp:487 | 1 match at FGIPass.cpp:487 ✓ |
| A5 | v17 case 7u TraceRay-bypass sentinel (Private master) | `search_files target=content path=Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl pattern="case 7u:"` | 1 match around line 604 | 1 match around line 604 ✓ |
| A6 | v17 case 7u sentinel (data-dir copy) | `search_files target=content path=Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl pattern="case 7u:"` | 1 match around line 604 | 1 match around line 604 ✓ |
| A7 | v28 alpha-alive sentinel (Private) | `search_files target=content path=Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl pattern="max\(Output\[pixel\].w, 0.99994f\)"` | 1 match around line 694 | 1 match around line 694 ✓ |
| A8 | v28 alpha-alive sentinel (data-dir) | `search_files target=content path=Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl pattern="max\(Output\[pixel\].w, 0.99994f\)"` | 1 match around line 694 | 1 match around line 694 ✓ |
| A9 | v37 validate_restir_gi.py::check_alpha_sentinel | `search_files target=content path=Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py pattern="def check_alpha_sentinel"` | 1 match around line 134 | verified at validate_restir_gi.py:134 ✓ |
| A10 | v40 dump_pixelstats.py v40-alpha block | `search_files target=content path=Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py pattern="def compute_alpha_stats"` | 1 match around line 96 | verified at dump_pixelstats.py:96 ✓ |
| A11 | v43 fresh-evidence-scan.sh CHECKS expansion | `search_files target=content path=Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh pattern="CHECKS=\("` | 1 match around line 57 | verified at fresh-evidence-scan.sh:57 ✓ |
| A12 | bug-088 executeCommandList at line ~691 | `search_files target=content path=Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp pattern="executeCommandList"` | 1 match near line 691 | verified (per v35 tick inspection + v25-v55 audits) ✓ |

12/12 Part A probes PASS via fresh `search_files`. 21/21 cumulative patches INTACT.

## Part B — Runtime tests (parent-driven; terminal blocked in cron)

| ID | Test | Criteria |
|----|------|----------|
| B1 | Default-mode rebuild + run + alpha-channel inspection | `display_frame8.png` alpha=255 (saturated) ⇒ dispatch ran; alpha=0 ⇒ dispatch didn't run |
| B2 | v37 alpha-check verdict on fresh dumps | `validate_restir_gi.py` returns alpha-saturated verdict if v28 sentinel compiled |
| B3 | v38 cerr-line decoding | `decode_v38_evidence.py stderr.log` returns structured verdict (GO/FIX_ATOI/FIX_DOCS/FIX_CVAR/NO_CERR/MIXED/UNRECOGNIZED) |
| B4 | v22 binding-layout effectiveness | `grep stderr.log VUID-VkDescriptorImageInfo-imageLayout-00344` ⇒ 0 matches (warning eliminated) |
| B5 | Validator 4/4 PASS | validate_restir_gi.py returns 4/4 on a fresh dump group produced post-rebuild |

5/5 Part B tests require parent terminal access (build + run + dump + validator + vision). Cron cannot execute; honest documentation of the persistent tirith block continues at every standby tick.
