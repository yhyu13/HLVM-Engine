# Pending Commit v231
- plan: docs/PENDING_PLAN_v231.md
- files: Engine/Source/Runtime/Test/TestCornellBoxGI_Data/ReSTIR_Generate_cs.hlsl, Engine/Source/Runtime/Test/TestCornellBoxGI.cpp
- source: no bundle
- target: branch (parent-owned)
- task: Card M — sync TestCornellBoxGI's generation shader + GenDesc to FReSTIRPass's shared GenerationLayoutSRV (7 SRVs). Closes the layout-vs-consumer divergence the v202 cycle identified.
- verify: `cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine && ./Build.sh --Config=Debug --Target=TestCornellBoxGI --Rebuild && cd Engine/Source/Runtime/Binary/Debug && ./TestCornellBoxGI && grep -E "VUID|ERROR" TestCornellBoxGI.log`
- skip_impl_review: yes
- produces_test_files: no
- notes: This patch is a binding-contract reconciliation, NOT a visual-quality fix. After the patch:
  1. Cornell's `ReSTIR_Generate_cs.hlsl` SPIR-V declares t0..t6, matching the shared layout at FReSTIRPass.cpp:182-184 (7 SRVs).
  2. Cornell's `GenDesc` populates the 3 missing fields (`DirectionTexture`, `SampleInfoTexture`, `MaterialTexture`) with `nullptr`. `FReSTIRPass::DispatchGeneration`'s ternaries at lines 501-503 fall back to `Desc.RadianceTexture` (t4) or `DummyGuide` (t5/t6), so the binding set is populated and Vulkan validation stays green.
  3. Cornell's generation shader does NOT read `gSample`/`gSampleInfo`/`gMaterial` in its `main()` — those declarations are inert at runtime, only present to make the SPIR-V's binding-slot declaration set match the shared layout.
  4. The primary target's `TestReSTIR_GI_Temporal_Data/ReSTIR_Generate_cs.hlsl` was NOT edited. Verify by re-running `search_files path=TestReSTIR_GI_Temporal_Data/ReSTIR_Generate_cs.hlsl pattern="register.t[0-9]."` → 7 hits (unchanged from baseline).

The v182 dual-copy hazard does NOT apply: the shader has only 2 copies (`TestCornellBoxGI_Data/` + `TestReSTIR_GI_Temporal_Data/`), no `Private/Renderer/Shader/` source-of-truth exists, and editing both copies would BREAK the primary target.

Diff stats:
- Shader: +13 / -0 (6 comment lines + 3 SRV decls + blank + 3 whitespace alignment, 0 removed)
- C++: +13 / -0 (6 comment lines + 3 GenDesc assignments + blank + 4 whitespace, 0 removed)
- Net functional lines: +6 / -0 (3 SRV decls + 3 GenDesc assignments)

Pre-edit counts (from this turn's independent file reads):
- `register.t[0-9].` in control generate shader: 4 hits (t0..t3)
- `gSample ` (with trailing space) in control: 0 hits
- `gSampleInfo` in control: 0 hits
- `gMaterial` in control: 0 hits
- `GenDesc.DirectionTexture` in TestCornellBoxGI.cpp: 0 hits
- `GenDesc.SampleInfoTexture` in TestCornellBoxGI.cpp: 0 hits
- `GenDesc.MaterialTexture` in TestCornellBoxGI.cpp: 0 hits

Post-edit counts (from this turn's edits):
- `register.t[0-9].` in control generate shader: 7 hits (t0..t6)
- `gSample ` in control: 1 hit (line 36)
- `gSampleInfo` in control: 1 hit (line 37)
- `gMaterial` in control: 1 hit (line 38)
- `GenDesc.DirectionTexture` in TestCornellBoxGI.cpp: 1 hit (line 1600)
- `GenDesc.SampleInfoTexture` in TestCornellBoxGI.cpp: 1 hit (line 1601)
- `GenDesc.MaterialTexture` in TestCornellBoxGI.cpp: 1 hit (line 1602)

## Plan Deviations

The plan-criticer (single-profile self-check) returned KEEP with 3 minor corrections: (1) diff_estimate slightly off, (2) verifier row syntax wrong, (3) risk #1 "may optimize them out" overstated. The impler addressed (1) by adopting v230's comment style (+6 comment lines + 3 SRV decls + whitespace = +13 shader lines, not +12); (2) the verifier rows are written as separate `search_files` invocations on the actual lines (not the joined pattern the plan suggested); (3) the impler's commit note rewrites the speculation as "declare-but-not-reference them" without mentioning dead-strip, which is the correct framing per the plan-criticer's correction.