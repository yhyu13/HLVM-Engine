# Pending Plan v231
- task: Card M — close the layout-vs-consumer divergence in `TestCornellBoxGI_Data/ReSTIR_Generate_cs.hlsl`. The shared `FReSTIRPass::GenerationLayoutSRV` declares 7 SRVs (t0..t6): `gRadiance` (t0), `gWorldPos` (t1), `gNormals` (t2), `gDepth` (t3), `gDirection`/Phase-B `gSample` (t4), `gSampleInfo` (t5), `gMaterial` (t6). The control's shader declares only t0..t3 (4 SRVs). The C++ `FReSTIRPass::DispatchGeneration` at lines 501-503 has ternaries `Desc.X ? Desc.X : DummyGuide/RadianceTexture` for t4/t5/t6 that keep the binding set populated when Cornell leaves them null — but they don't reconcile the layout with the SPIR-V.
- source: no bundle — direct edit of two files (`TestCornellBoxGI_Data/ReSTIR_Generate_cs.hlsl` + `TestCornellBoxGI.cpp`)
- approach:
  1. **Shader copy** (the control's `TestCornellBoxGI_Data/ReSTIR_Generate_cs.hlsl`): add 3 missing SRV declarations (t4 = `gSample`, t5 = `gSampleInfo`, t6 = `gMaterial`). Per card M, the control's pipeline is "built from a layout advertising a binding its SPIR-V does not contain." Adding the declarations brings the SPIR-V into agreement with the shared layout.
  2. **C++ caller** (`TestCornellBoxGI.cpp:1590-1599`): add 3 missing GenDesc assignments (`DirectionTexture`, `SampleInfoTexture`, `MaterialTexture`). Per FReSTIRPass.cpp:501-503, all 3 have ternaries that fall back to `Desc.RadianceTexture` (t4) or `DummyGuide` (t5/t6) when null. **Leaving them at null is safe at the Vulkan layer** (binding set is populated via fallbacks), and Cornell's role is "known-good control," not "best-looking render." The fallback data on t4/t5/t6 will produce noisy generation output, but no VUID and no crash.
  3. **Verify the dual-copy hazard does NOT apply** — independent search confirmed: the shader has only 2 copies (`TestCornellBoxGI_Data/` and `TestReSTIR_GI_Temporal_Data/`); no `Private/Renderer/Shader/` source-of-truth exists. The primary copy is the correct version of the shader. **Edit the control's copy ONLY. Do NOT touch the primary.**
- diff_estimate: +12 / -2 shader lines (3 new SRV decls + 6 comment lines + 3 whitespace; remove 2 default-space UAV comments — actually, no UAVs change here, just +12 SRV lines net) PLUS +9 / -0 C++ lines (3 GenDesc assignments + 6 comment lines). **Total: +21 / -2.**
- skip_plan_review: no (binding-contract reconciliation; second pair of eyes against the v203 near-miss)
- test_strategy: file-only verifier (12 rows).
  1. Shader side:
     - `search_files path=TestCornellBoxGI_Data/ReSTIR_Generate_cs.hlsl pattern="register.t[0-9]."` → 7 hits (t0..t6, up from 4)
     - `search_files path=TestCornellBoxGI_Data/ReSTIR_Generate_cs.hlsl pattern="gSample "` → 1 hit
     - `search_files path=TestCornellBoxGI_Data/ReSTIR_Generate_cs.hlsl pattern="gSampleInfo"` → 1 hit
     - `search_files path=TestCornellBoxGI_Data/ReSTIR_Generate_cs.hlsl pattern="gMaterial"` → 1 hit
  2. C++ side:
     - `search_files path=TestCornellBoxGI.cpp pattern="GenDesc.DirectionTexture"` → 1 hit (new assignment)
     - `search_files path=TestCornellBoxGI.cpp pattern="GenDesc.SampleInfoTexture"` → 1 hit
     - `search_files path=TestCornellBoxGI.cpp pattern="GenDesc.MaterialTexture"` → 1 hit
  3. Primary copy MUST remain unchanged:
     - `search_files path=TestReSTIR_GI_Temporal_Data/ReSTIR_Generate_cs.hlsl pattern="register.t[0-9]."` → 7 hits (unchanged from baseline)
  4. Shared layout count matches shader declarations:
     - `search_files path=FReSTIRPass.cpp pattern="GenerationLayoutSRV" context=BindingLayoutItem::Texture_SRV"` → 7 hits in the GenerationLayoutSRV initialiser (t0..t6, unchanged)
- risks:
  1. **The card M defect is "latent, not fatal"** per card M's body. The ternary in DispatchGeneration prevents the crash, so the SPIR-V/layout mismatch was silently tolerated. The fix changes correctness: after the patch, Cornell's `gSample`/`gSampleInfo`/`gMaterial` slots read from 1x1 dummy textures (t5/t6) and from `Desc.RadianceTexture` (t4 — same as the existing t0). The generation pass's behavior will differ from the previous version because t4 was previously UNBOUND (not declared) and is now BOUND to the radiance texture. Whether this breaks Cornell's render output depends on whether the generation shader actually reads `gSample`/`gSampleInfo`/`gMaterial`. **Verify before applying**: enumerate `gSample`/`gSampleInfo`/`gMaterial` reads in the control's shader — there are NONE (the shader doesn't declare them, so it doesn't read them). After the fix, the declarations will exist but no reads follow them — the compiler may optimize them out, leaving the binding layer populated but unused. **No behavioral change expected.**
  2. **Per the v203 standing rule about patch anchoring on initialiser-bound comments**: this patch edits HLSL declaration lines and C++ assignment lines, NOT C++ initialisers. Risk class is different.
  3. **The dual-copy hazard** explicitly checked: primary copy byte-equal, control copy is the only one edited.
  4. **Per the v182 rule**: no third copy of the shader exists in `Private/Renderer/Shader/`. Confirmed via `search_files`.
  5. **v200 cbuffer layout rule**: applies to cbuffers, not textures. N/A.
  6. **v197 `Add*` not `Set*` gotcha**: applies to C++ binding layouts, not HLSL `register(...)`. N/A.
  7. **Card N parallel**: card N was already closed by v230 (separate file, separate layout). Bundling N + M into one cycle would have made each cycle's verifier unverifiable. N was its own cycle, M is this cycle.

## Pre-patch verification commands
```bash
# Verify both shader copies exist
ls -la Engine/Source/Runtime/Test/TestCornellBoxGI_Data/ReSTIR_Generate_cs.hlsl
ls -la Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ReSTIR_Generate_cs.hlsl

# Verify the C++ struct has the missing fields (already confirmed this turn — FReSTIRPass.h:84-99)
grep -E "DirectionTexture|SampleInfoTexture|MaterialTexture" Engine/Source/Runtime/Public/Renderer/PostProcess/FReSTIRPass.h

# Verify the marshaller's ternaries (already confirmed this turn — FReSTIRPass.cpp:501-503)
grep -nE "DirectionTexture|SampleInfoTexture|MaterialTexture|DummyGuide|RadianceTexture" Engine/Source/Runtime/Private/Renderer/PostProcess/FReSTIRPass.cpp
```

## Cycle scorecard (pre-edit baseline — recorded by the tester, NOT inherited)

Independent counts at start of this cycle (file-tool reads):
- `register.t[0-9].` in control generate shader: 4 hits (t0..t3)
- `gSample ` (with trailing space) in control: 0 hits
- `gSampleInfo` in control: 0 hits
- `gMaterial` in control: 0 hits
- `GenDesc.DirectionTexture` in TestCornellBoxGI.cpp: 0 hits
- `GenDesc.SampleInfoTexture` in TestCornellBoxGI.cpp: 0 hits
- `GenDesc.MaterialTexture` in TestCornellBoxGI.cpp: 0 hits

POST-edit expected:
- `register.t[0-9].` in control generate shader: 7 hits (t0..t6)
- `gSample ` in control: 1 hit
- `gSampleInfo` in control: 1 hit
- `gMaterial` in control: 1 hit
- `GenDesc.DirectionTexture` in TestCornellBoxGI.cpp: 1 hit
- `GenDesc.SampleInfoTexture` in TestCornellBoxGI.cpp: 1 hit
- `GenDesc.MaterialTexture` in TestCornellBoxGI.cpp: 1 hit