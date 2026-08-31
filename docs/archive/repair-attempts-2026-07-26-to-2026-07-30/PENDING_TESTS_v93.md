# Pending Tests v93
- tests: docs/PENDING_COMMIT_v93.md
- files: docs/PENDING_*_v93.md (markers only — no test files produced in this tick)
- tester: tester (single-profile, file-only runspace)
- timestamp: 2026-07-28T23:32Z

## Part A — file-only probes (3/3 PASS)
- [x] P1: GIPathTracing.hlsl:88 in `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` shows `RWTexture2D<float4> Output : register(u0);` (default space0, no `space1`)
- [x] P1b: GIPathTracing.hlsl:88 in `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` shows the identical declaration (no `space1` either)
- [x] P2: FRayTracingPipeline.cpp:148-153 shows `PipelineDesc.globalBindingLayouts = { BindingLayout };` plus optional `BindlessLayout` push. UAVBindingLayout never referenced.
- [x] P3a: FReSTIRPass.cpp:246-247 shows `PipelineDesc.addBindingLayout(TemporalLayoutSRV);` AND `PipelineDesc.addBindingLayout(TemporalLayoutUAV);` — both layouts registered.
- [x] P3b: ReSTIR_Temporal_cs.hlsl:32-33 shows `RWTexture2D<float4> gOutReservoir0 : register(u0, space1);` AND `gOutReservoir1 : register(u1, space1);` confirming the correct sibling shape.

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
Part A confirms the file-only root cause. Part B's B7 (spirv-cross reflection of compiled GIPathTracing SPIR-V) is the single decisive terminal probe that would either confirm or falsify this v93 diagnosis without rebuilding the project. If `spirv-cross --reflect` on `GIPathTracing.spv` shows `Output` at set=0 binding=0 (NOT set=1), then v93's diagnosis is correct and the parent can apply the shader-side `, space1` fix and the FRayTracingPipeline-side UAVBindingLayout registration fix in one commit and rebuild.
