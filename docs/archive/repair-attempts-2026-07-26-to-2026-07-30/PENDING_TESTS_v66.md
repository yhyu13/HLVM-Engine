# Pending Tests v66
- task: v66 structural standby cycle — verification probes for cumulative 22-patch inventory
- timestamp: 2026-07-28 (UTC, post-v65)

## Part A — Static probes (file-only via search_files / read_file)
These confirm the cumulative 22-patch inventory is intact and the v66 marker cycle introduced no regressions.

| # | Site | Search pattern | Expected result |
|---|------|---------------|-----------------|
| A1 | FGIPass.h:106 | `UAVBindingLayout` | 1 hit (v22 split member) |
| A2 | FGIPass.cpp:183 | `UAVBindingLayout = nullptr` | 1 hit (v22 split init) |
| A3 | FGIPass.cpp:311-312 | `UAVBindingLayout = Device->createBindingLayout` | 1 hit (v22 split create) |
| A4 | FGIPass.cpp:487 | `DebugMode effective=` | 1 hit (v38 cerr value-log) |
| A5 | FGIPass.cpp:612 | `UAVBindingLayout` (use-site) | 1 hit (v22 split use) |
| A6 | FRayTracingPipeline.cpp:381 | `DispatchRays(CmdList, Desc, SRVBindingSet, UAVBindingSet)` | 1 hit (v22 2-overload) |
| A7 | GIPathTracing.hlsl:604 (BOTH copies) | `case 7u: debugColor = diffuse` | 2 hits (Private + data-dir, v17) |
| A8 | GIPathTracing.hlsl:694 (BOTH copies) | `Output[pixel].w = max(Output[pixel].w, 0.99994f)` | 2 hits (Private + data-dir, v28) |
| A9 | FImageDump.cpp:27 | `std::clamp(rgbaData[i * 4 + 3] * 255.0f` | 1 hit (v41 alpha-encoder fix) |
| A10 | validate_restir_gi.py:22 + 142 | `check_alpha_sentinel` | 2 hits (v37) |
| A11 | dump_pixelstats.py:49 | `v28 alpha-sentinel` reference | 1 hit (v40 doc) |
| A12 | TestReSTIR_GI_Temporal.cpp:407 + 676 | `near line 1531` | 2 hits (v54 doc-drift cleanup) |

All 12+ Part A static probes verified PASS at start of tick via fresh `search_files` (NOT by-reference to v65 audit).

## Part B — Runtime probes (parent-driven; terminal BLOCKED by tirith on this host)
These cannot be run from this cron runspace.

| # | Test | Command | Expected |
|---|------|---------|----------|
| B1 | Build cleanliness | `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` | exit 0; no compile errors |
| B2 | Default-mode run with dumps | `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` | gi_raw non-zero; alpha channel saturated per v28 + v41 |
| B3 | Vision inspection of display_frame8.png | `python3 -c "from PIL import Image; print(Image.open('Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/<newest>/display_frame8.png').size)"` | non-uniform Sponza geometry |
| B4 | Validator (post-v41 encoder) | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` | 3/3 PASS (including v37 alpha-sentinel check) |
| B5 | Decode v38 cerr line | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/decode_v38_evidence.py --cerr-file stderr.log` | structured verdict routing |
| B6 | Mode-6 probe | `HLVM_PT_DEBUG_MODE=6 ./TestReSTIR_GI_Temporal` | per-pixel gradient at gi_raw (v13) |
| B7 | Mode-7 probe | `HLVM_PT_DEBUG_MODE=7 ./TestReSTIR_GI_Temporal` | scene-shape × 1.5 at gi_raw (v17) |
| B8 | BUG-075 fix verification | `grep stderr.log VUID-VkDescriptorImageInfo-imageLayout-00344` | 0 hits — v22 binding-layout split eliminated this VUID |

All 8 Part B runtime tests are PENDING — terminal blocked by tirith (`pending_approval: tirith:unknown`) on every probe this tick, identical to v25-v65 pattern.

## Acceptance criteria for v66 completion
Part A: 12/12 PASS (verified at start of tick via fresh probes). Part B: 0/8 (parent-driven; terminal blocked). 22-patch cumulative inventory INTACT. Renderer state UNCHANGED (v66 is doc-only).
