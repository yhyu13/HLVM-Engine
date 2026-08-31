# Pending Plan Review v231
- plan: docs/PENDING_PLAN_v231.md
- verdict: KEEP
- reviewer: plan-criticer (six-role pipeline role #2)
- timestamp: 2026-08-22T02:00:00Z

## Design soundness

The plan correctly identifies the divergence: `FReSTIRPass::GenerationLayoutSRV` declares 7 SRVs (t0..t6 per FReSTIRPass.cpp:182-184) but the control's `ReSTIR_Generate_cs.hlsl` declares only t0..t3 (lines 25-28 verified this turn via `search_files`). The plan's proposed fix is the same shape as the v230 cycle: sync shader declarations + sync C++ caller assignments. The dual-copy hazard is correctly identified (primary byte-equal, control copy only edited).

Risk #1 is the substantive one and the plan handles it correctly: the `gSample`/`gSampleInfo`/`gMaterial` symbols are DECLARED but NEVER READ in the control's shader (the shader only declares them after the patch, but its `main()` doesn't reference them). After the patch, the declarations exist, the binding layer is populated (Desc.DirectionTexture null → ternary falls back to RadianceTexture; Desc.SampleInfoTexture null → DummyGuide; Desc.MaterialTexture null → DummyGuide), and the SPIR-V matches the layout. No behavioural change.

## Plan completeness

Three corrections to be made before impl:

1. **Diff_estimate is slightly off**. The plan claims "+12/-2 shader lines" but 3 new SRV declarations + 6 comment lines = 9 net (3 + 6 - 0 - 0 = 9), not 12. The actual number is +9 shader lines if the impler mirrors the primary's verbatim 1-line declarations without comments, or up to +15 if the impler adds the explanatory comment block from v230. Either is acceptable, but the plan should say "+9 to +15" or commit to one. Recommend: mirror v230's style (+9 SRV lines + 6 comment lines = +15 total) for consistency with the cycle that just landed.

2. **Test_strategy row 4 is "GenerationLayoutSRV" → 7 hits but the search pattern has wrong syntax**. The plan writes `search_files path=FReSTIRPass.cpp pattern="GenerationLayoutSRV" context=BindingLayoutItem::Texture_SRV"` which is not a valid `search_files` invocation. The correct verifier row would be `search_files path=FReSTIRPass.cpp pattern="BindingLayoutItem::Texture_SRV" context=3` and manually count 7 hits within the GenerationLayoutSRV block. Or simply verify that the generation layout count (7 SRVs + 2 UAVs) matches the primary's declarations. The plan's INTENT is correct but the SYNTAX is wrong.

3. **Risk #1's "may optimize them out" concern is overstated**. HLSL DOES allow dead-strip of unused declarations, but `nvrhi`'s binding layout is created BEFORE the SPIR-V is compiled, so the layout's `Texture_SRV(4/5/6)` declarations are independent of whether the shader's SPIR-V references them. The binding set will have 7 slots, populated via ternaries; whether the SPIR-V references slot 4/5/6 is irrelevant to Vulkan validation. This is fine — but the plan's wording "compiler may optimize them out" is misleading. Recommend: drop the speculation, state simply "the shader doesn't read these symbols; the binding set will be populated but inert."

These are minor — not FIX-worthy. Approving KEEP with the understanding that the impler picks one of the two comment styles (matching v230's style is the safer bet for consistency).

## Single-profile caveat

Same as v230 — the plan-criticer is the same model as the planner. The KEEP verdict is a self-check, not a fresh-eyes review. The 3 corrections are picked up by the impler at apply-time.