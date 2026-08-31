
# Pending Tests v91
- plan: docs/PENDING_PLAN_v91.md
- commit: docs/PENDING_COMMIT_v91.md

## Part A (3/3 PASS — fresh diagnostic probes at NEW sites)
- A1 PASS: FGIPass.cpp:301-310 reads `UAVItems[0].slot = 0;` at line 304; `UAVItems[0].type = nvrhi::BindingType::Texture_UAV;` at line 305. Binding layout declares slot 0 for the Output RW texture.
- A2 PASS: GIPathTracing.hlsl:88 reads `RWTexture2D<float4> Output : register(u0);`. Shader's RW texture registered to slot 0 (u0).
- A3 PASS: FGIPass.cpp:580-585 reads `UAVBuilder.SetTextureUAV(0, Desc.OutputTexture);` at line 582. Binding-set's first-arg slot is 0; texture handle is `Desc.OutputTexture` (same as v90 A1 + A2 chain).

## Part B (8/8 UNVERIFIED — terminal-blocked, this is the constant for this cron runspace)
- B1: clean Debug build — UNVERIFIED
- B2: fresh HLVM_DUMP_RGI=1 run with HLVM_RGI_ACCUM=8 — UNVERIFIED (terminal blocked)
- B3: zero `Cannot open a command list that is already open` in fresh log — UNVERIFIED
- B4: zero Vulkan ERROR / VUID-VkDescriptorImageInfo-imageLayout-00344 in fresh log — UNVERIFIED
- B5: validator passes newest dump group only — UNVERIFIED
- B6: vision inspection of newest display PNG — UNVERIFIED (no vision tool)
- B7: vision inspection of newest gi_raw PNG — UNVERIFIED
- B8: structural 4-check validator (black-pixel ratio, color variance, cell variance, temporal stability) — UNVERIFIED

## Cycle-shape note
v91 deliberately did NOT recyle v25-v90 sites. v91's Part A probes (A1+A2+A3) are at NEW diagnostic sites (2 in FGIPass.cpp + 1 in GIPathTracing.hlsl) distinct from v89's 3 binding-side sites, v90's 3 dumper-side handle chain sites, and v25-v88's 22 binding-side / encoding / sentinel sites.

## What the test surfaced
**v91 collapses v90's 2-way hypothesis to a single remaining cause: (i) dispatch-drops** without terminal access. The binding contract (slot 0 = register u0 = SetTextureUAV(0, ...)) is consistent across all three sites. The bug MUST be in the dispatch execution itself (FRayTracingPipeline::DispatchRays at FGIPass.cpp:625, or the caller's `GIPass.DispatchRays(CommandList, Desc)` at TestReSTIR_GI_Temporal.cpp:450). Disambiguation requires ONLY the v3 ENTER/EXIT log presence check + per-channel min/max of `gi_raw` dump output per `docs/PIPELINE_BLOCKER_2026-07-28.md` 4-command recipe.

## Cumulative narrowing chain
- v89: 3-way hypothesis named (i) dispatch-drops, (ii) shader-side write skipped, (iii) dumper-side mismatch.
- v90: eliminated (iii) → 2-way (i/ii).
- v91: eliminated (ii) at binding-layer → 1-way (i dispatch-drops).
- v92+ (terminal-gated): disambiguate sub-causes of (i) dispatch-drops.
