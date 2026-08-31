# Pending Tests v94
- tests: docs/PENDING_COMMIT_v94.md
- files: docs/PENDING_*_v94.md (markers only — no test files produced in this tick)
- tester: tester (single-profile, file-only runspace)
- timestamp: 2026-07-28T23:50Z

## Part A — file-only spot-checks (6/6 PASS, cross-tick verification of v93 findings)
- [x] P1.intact: `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl:88` still reads `RWTexture2D<float4> Output : register(u0);` (no space1) — verified this tick via search_files
- [x] P1b.intact: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl:88` still reads identically — verified this tick
- [x] P2.intact: `Engine/Source/Runtime/Private/Renderer/RayTracing/FRayTracingPipeline.cpp:149` still reads `PipelineDesc.globalBindingLayouts = { BindingLayout };` (no UAVBindingLayout push) — verified this tick
- [x] P3a.intact: `Engine/Source/Runtime/Private/Renderer/PostProcess/FReSTIRPass.cpp` still registers both SRV+UAV layouts and references space1 (lines 150-154 + 385-388) — verified via search_files
- [x] P3b.intact: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl:32-33` still declares `register(u0, space1)` / `register(u1, space1)` — verified via search_files
- [x] v28 alpha-sentinel intact: `GIPathTracing.hlsl:694` still reads `Output[pixel].w = max(Output[pixel].w, 0.99994f);` in BOTH Private+Data copies — verified this tick

## Part B — terminal-required probes (8/8 UNVERIFIED, terminal blocked)
- [ ] B1: presence/absence of `FGIPass::DispatchRays ENTER` log at FGIPass.cpp:527
- [ ] B2: presence/absence of `FGIPass::DispatchRays EXIT` log at FGIPass.cpp:631
- [ ] B3: per-channel min/max of `gi_raw` from latest dump
- [ ] B4: Vulkan VUID error/warning count in fresh log
- [ ] B5: `python3 validate_restir_gi.py` exit code on newest stamp group
- [ ] B6: `nvrhi::setShaderTable`'s compiled pipeline layout descriptor-set count (1 vs 2)
- [ ] B7: `spirv-cross` reflection of compiled GIPathTracing.hlsl showing `set=0 binding=0 type=StorageImage` for Output
- [ ] B8: visual diff: does display dump at frame 8 show Sponza geometry?

## Conclusion
Part A confirms v93's diagnosis is intact on disk (not stale). Part B's 8 probes remain UNVERIFIED because terminal is structurally blocked in this cron's runspace. The cron pivot this tick is to **stop looping on `restir-gi-fix`** until the parent supplies terminal evidence per `PIPELINE_RUNSPACE_BLOCKED_2026-07-28.md`.