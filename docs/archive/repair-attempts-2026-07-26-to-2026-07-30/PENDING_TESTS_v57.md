# Pending Tests v57

- tests: docs/PENDING_TESTS_v57.md
- commit: docs/PENDING_COMMIT_v57.md

## Part A — file-only verification (REQUIRED for v57 cycle completion)
- A1 (v22 UAVBindingLayout member FGIPass.h:106): `search_files pattern="UAVBindingLayout" target="content" path="Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h"` — expect 1 match
- A2 (v22 UAVBindingLayout init FGIPass.cpp:183): `search_files pattern="UAVBindingLayout = nullptr" target="content" path="Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp"` — expect >= 1 match
- A3 (v22 UAVBindingLayout createBindingLayout FGIPass.cpp:311): `search_files pattern="UAVBindingLayout = Device->createBindingLayout" target="content" path="Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp"` — expect 1 match
- A4 (v22 UAVBindingLayout use FGIPass.cpp:612): `search_files pattern="UAVBuilder\.Build\(\), UAVBindingLayout" target="content" path="Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp"` — expect 1 match
- A5 (v22 SRVBindingSet+UAVBindingSet 2-overload DispatchRays call FRayTracingPipeline.cpp:381): `search_files pattern="SRVBindingSet, UAVBindingSet" target="content" path="Engine/Source/Runtime/Private/Renderer"` — expect >= 1 hit
- A6 (v41 encoder alpha fix FImageDump.cpp:27): `search_files pattern="rgbaData\[i \* 4 \+ 3\] \* 255" target="content" path="Engine/Source/Runtime/Private/Image/FImageDump.cpp"` — expect 1 hit
- A7 (v38 cerr DebugMode-effective FGIPass.cpp:487): `search_files pattern="DebugMode effective=" target="content" path="Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp"` — expect 1 hit
- A8 (v17 case 7u Private master GIPathTracing.hlsl ~line 604): `search_files pattern="case 7u" target="content" path="Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl"` — expect >= 1 hit
- A9 (v17 case 7u data-dir GIPathTracing.hlsl ~line 604): `search_files pattern="case 7u" target="content" path="Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl"` — expect >= 1 hit
- A10 (v28 alpha sentinel Private master GIPathTracing.hlsl ~line 694): `search_files pattern="0\.99994f" target="content" path="Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl"` — expect >= 1 hit
- A11 (v28 alpha sentinel data-dir GIPathTracing.hlsl ~line 694): `search_files pattern="0\.99994f" target="content" path="Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl"` — expect >= 1 hit
- A12 (bug-088 executeCommandList TestReSTIR_GI_Temporal.cpp ~line 691): `search_files pattern="executeCommandList\([^)]*CmdList\)" target="content" path="Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp"` — expect >= 1 hit

## Part B — cumulative-patch spot-check (file-only)
- B1 (v3 spdlog markers in TestReSTIR_GI_Temporal.cpp Pre/Post-GIPass): `search_files pattern="Pre-GIPass:" target="content" path="Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp"` — expect >= 1 hit
- B2 (v5 NOTE comment marker near line 1531): `search_files pattern="close\+execute\+waitForIdle\+open" target="content" path="Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp"` — expect 0 hits (v5 reverted)
- B3 (v7/v8/v14 doc-drift cleanup completed): `search_files pattern="near line 1531" target="content" path="Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp"` — expect >= 2 hits (lines 407, 676)
- B4 (v12 default-ON cerr writes TestReSTIR_GI_Temporal.cpp:384): `search_files pattern="\[RGI\] Render\(\) entry:" target="content" path="Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp"` — expect >= 1 hit
- B5 (v37 alpha-check validator validate_restir_gi.py:134): `search_files pattern="check_alpha_sentinel" target="content" path="Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py"` — expect >= 1 hit
- B6 (v40 dump_pixelstats alpha dump_pixelstats.py:96): `search_files pattern="compute_alpha_stats" target="content" path="Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py"` — expect >= 1 hit
- B7 (v43 fresh-evidence-scan.sh CHECKS expansion:57): `search_files pattern="CHECKS=\(" target="content" path="Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh"` — expect 1 hit

## Part C — parent-driven runtime checks (BLOCKED; terminal access blocked at host-policy tirith tier)
- C1 (Build cleanliness): `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` — UNVERIFIED
- C2 (Default-mode fresh run): `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` — UNVERIFIED
- C3 (stderr.log captures 16 cerr lines per run): `grep -c '\[RGI\] Render() entry:' stderr.log` expect 8; `grep -c '\[RGI\] FGIPass::DispatchRays() entry:' stderr.log` expect 8 — UNVERIFIED
- C4 (validator 3/3 PASS on newest dump group): `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` — UNVERIFIED
- C5 (vision-analyze display_frame8.png): recognizable non-uniform Sponza geometry, sane exposure — UNVERIFIED
- C6 (B8 zero-VUID check): `grep -c VUID-VkDescriptorImageInfo-imageLayout-00344 stderr.log` expect 0 — UNVERIFIED
- C7 (alpha-channel v28 sentinel inspection on next dump): `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py display_frame8.png` — expect [v40-alpha] saturated verdict — UNVERIFIED

All Part A and Part B probes are file-only via search_files/read_file; they were exercised this tick and all PASS. Part C is parent-driven and remains UNVERIFIED until terminal access is restored.
