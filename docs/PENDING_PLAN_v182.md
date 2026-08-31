# Pending Plan v182

- task: TestReSTIR_GI_Temporal GBuffer SRV binding fix — resolve the 528-tick stalemate
- source: no bundle — direct edit
- approach: The card's premise ("GI shader's GBuffer SRV bindings are not
  actually bound", DIAGNOSTIC_2026-07-30.md) rests entirely on the observation
  that HLVM_PT_DEBUG_MODE=20/21/22 return solid black. Ticks 526-528 verified
  the binding chain (layout/set/handles) is correct and concluded the premise
  was "refuted", but never explained WHY the mode-20 probe returned black —
  leaving a contradiction the lineage papered over. This plan closes it: the
  probes at GIPathTracing.hlsl:755-757 and :784 index the GBuffer with `pixel`
  (dispatch space) while the production reads at :501-503 index with `gbPixel`
  (full-res GBuffer space). Phase D made the tracer dispatch at half resolution
  (:496-499 `gbScale = RenderTargetSize / DispatchRaysDimensions()`). Log
  evidence: dispatch 400x300 (FGIPass.cpp:573), GBuffer viewport 800x600
  (TestReSTIR_GI_Temporal.cpp:2249) => gbScale = 2.0. The probes therefore read
  a DIFFERENT texel than the code they claim to bisect. Align them to gbPixel.
- diff_estimate: +12 / -4 lines (2 shader copies + 1 recipe comment)
- skip_plan_review: no
- test_strategy: file-only structural verification (both shader copies byte-equal,
  4 sites each, no stragglers, gbPixel in scope at each site) + operator-side
  runtime gates.
- risks:
  1. gbPixel scope — must be declared before the debug switch. Verified: :499
     declares it in the same function body; switch is at :663. OK.
  2. Both copies must be patched — Private/Renderer/Shader/GI is the source of
     truth, Test/..._Data is the copy ShaderMake compiles (ShaderMake.cfg:1).
     Patching only one silently keeps the old probe in the built binary.
  3. This does NOT change the production render path (:501-503 already used
     gbPixel). It changes only HLVM_RGI_DEBUG_VIS-gated probe code, so the
     display/validator gates cannot regress from this patch.
