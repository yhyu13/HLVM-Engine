# Pending Plan v93
- task: restir-gi-fix — file-only smoking-gun for hypothesis (i) dispatch-drops
- source: no bundle — direct read
- approach: Document 3 NEW file-only probes that deterministically localize the bug to v22-spit incompleteness in FGIPass: (P1) GIPathTracing.hlsl:88 declares `Output : register(u0)` (default space0), but the v22 split intends `register(u0, space1)`; (P2) FGIPass.cpp:311 builds UAVBindingLayout (separate from RTPipeline's SRV BindingLayout) but FRayTracingPipeline.cpp:149 only registers `globalBindingLayouts = { BindingLayout }` — the UAV layout is NEVER registered with the pipeline; (P3) ReSTIR_Temporal pass at FReSTIRPass.cpp:246-247 BOTH registers SRV+UAV layouts AND its shader at ReSTIR_Temporal_cs.hlsl:32-33 declares `register(u0, space1)` — the GI shader is the missing-piece sibling.
- diff_estimate: +0/-0 source-code lines; +~30 lines across 6 PENDING_*_v93.md markers + HEALTH append
- skip_plan_review: no
- test_strategy: tester re-confirms P1/P2/P3 via read_file cross-tick verification of unchanged source between v93 and parent run
- risks: The diagnosis is derived from file-only reasoning, not from a captured Vulkan VUID. Terminal validation per `PIPELINE_BLOCKER_2026-07-28.md` 4-command recipe remains required before any patch lands. Risk of premature patch — explicitly deferred to a parent-driven terminal run.

## Cycle-meaning
v93 transition: v92 PARTIAL_KEEP_DIVERGENCE → v93 ROOT_CAUSE_NAMED. The 1-way hypothesis (i) is now file-only deterministically identified as "v22 split is half-applied to FGIPass: UAV layout created and dispatched but never registered with the RTPipeline, and the GI shader's UAVs are at space0 not space1." This matches the gpu-rendering-bisect-debug anti-pattern #7 (dump vs. shader-sees-different-data) at descriptor-set-binding level. The parent can verify this hypothesis in 10 seconds with a captured nvrhi runtime log if available, or 120s by adding `spirv-cross`-style descriptor-set printouts to the shader.
