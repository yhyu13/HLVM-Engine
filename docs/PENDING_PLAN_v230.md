# Pending Plan v230
- task: Card N — close the layout-vs-consumer divergence in `TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl`. The shared `FReSTIRPass::TemporalLayoutSRV` declares 16 SRVs + 1 RT AS (t0..t16), but the control's shader declares only t0..t7; the shared `TemporalLayoutUAV` declares 4 UAVs in `space1` (registers u384..u387 → SPIR-V set 1), but the control's shader declares only `register(u0)`/`register(u1)` in the DEFAULT space. This means every temporal dispatch from `TestCornellBoxGI.cpp` is built against a binding layout the SPIR-V does not actually contain.
- source: no bundle — direct edit of two files (`TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl` + `TestCornellBoxGI.cpp`)
- approach: This cycle is the FIX revision incorporating the plan-criticer's 3 corrections.
  1. **Shader copy** (the control's `TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl`): add 8 missing SRV declarations (t8..t15 + g_bvh at t16) mirroring the primary; add `, space1` to the 2 existing UAV declarations; add 2 missing UAV declarations (u2, u3) with `, space1`.
  2. **C++ caller** (`TestCornellBoxGI.cpp:1660-1675`): assign the 6 missing fields on `TempDesc` so the binding set is populated with caller-supplied textures **where Cornell already has them** and is left at default (null → falls back to dummy) **where Cornell doesn't have them**. Per the FReSTIRPass header (FReSTIRPass.h:106-145) and DispatchTemporal's ternaries (FReSTIRPass.cpp:606/609/616-619), every missing field has a dummy fallback — leaving nulls is safe at the Vulkan layer but data-starves the temporal pass on those slots. Cornell's role is "known-good control," not "best-looking render," so the data-starved path is acceptable for THIS cycle.
  3. **Verify the dual-copy hazard does NOT apply** — independent search confirmed the shader exists in only TWO copies (`TestCornellBoxGI_Data/` and `TestReSTIR_GI_Temporal_Data/`). The primary copy at `TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl` is **the** correct version of the shader; the control's copy is the diverged one. **Edit the control's copy ONLY. Do NOT touch the primary.**
- diff_estimate: +15 / -2 shader lines (8 new SRV decls + 2 default-space→space1 changes + 2 new UAV decls + 3 whitespace; remove 2 default-space decls) PLUS +6 / -0 C++ lines (one TempDesc assignment per missing field — left null where Cornell lacks the texture, set where it has one). **Total: +21 / -2.**
- skip_plan_review: no (the divergence touches a contract — bindings-to-shaders — and a second pair of eyes is the cheapest insurance against the v203 near-miss)
- test_strategy: file-only verifier (17 rows).
  1. Shader side:
     - `search_files pattern="register\(t8\)" path=TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl` → 1 hit
     - `search_files pattern="register\(t16\)" path=TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl` → 1 hit (g_bvh)
     - `search_files pattern="register\(u0, space1\)" path=TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl` → 1 hit
     - `search_files pattern="register\(u3, space1\)" path=TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl` → 1 hit
     - `search_files pattern="register\(u[0-3]\)" path=TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl` → 4 hits (one per UAV)
     - `search_files pattern="register\(t[0-9]\)" path=TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl` → 8 hits (t0..t7, unchanged)
     - `search_files pattern="register\(t1[0-6]\)" path=TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl` → 7 hits (t10..t16, new)
  2. C++ side:
     - `search_files pattern="TempDesc.(Current|History)Reservoir2|WorldPosTexture|MaterialTexture|PrevWorldPosTexture|PrevMaterialTexture" path=TestCornellBoxGI.cpp` → 6 hits
  3. Primary copy MUST remain unchanged:
     - `search_files pattern="register\(t[0-9]\)" path=TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl` → **same byte count before and after the edit** (control cycle did not perturb the primary)
  4. Shared layout count matches shader declarations:
     - `search_files pattern="Texture_SRV\(" path=FReSTIRPass.cpp` → 17 hits (16 SRVs + 1 RT AS), same as primary's t0..t16
     - `search_files pattern="Texture_UAV\(" path=FReSTIRPass.cpp` → for `TemporalLayoutUAV` block only → 4 hits
  5. The C++ marshalling at FReSTIRPass.cpp:604-619 (16 SRV bindings) is unchanged from the plan's perspective.
- risks:
  1. **C++ side MUST be edited too** — the plan's original risk #1 confirmed critical: 6 of the 16 binding slots will have null caller-supplied textures in Cornell. Vulkan validation will NOT fire (because of the ternary fallbacks at FReSTIRPass.cpp:606/609/616-619), but the temporal pass will be data-starved on those slots. Acceptable for THIS cycle because Cornell's role is "known-good control," not "best-looking render." **Document this trade-off in the commit manifest** so future ticks understand why Cornell's temporal output may look noisy on the affected slots.
  2. **Shader duplicate**: per v182 hazard, `ReSTIR_Temporal_cs.hlsl` exists in both `Engine/Source/Runtime/Test/TestCornellBoxGI_Data/` (the copy this cycle edits) and `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/` (the primary, **must NOT be edited**). No third copy exists in `Private/Renderer/Shader/` (confirmed via `search_files pattern="ReSTIR_Temporal_cs.hlsl"` returning only the 2 data-dir copies).
  3. **Card M parallel**: card M (t4 generation shader) is in the same control-data-dir but a DIFFERENT file (`ReSTIR_Generate_cs.hlsl`). Do NOT bundle — different file, different pipeline, different layout.
  4. **The "controlled positive" inside the control's directory** (ReSTIR_Generate_cs.hlsl at `:36-37` uses `register(u0, space1)`) confirms this is a copy-the-primary-edit-but-never-finished, not a deliberate style choice.
  5. **VRAM cost of new bindings**: t8..t15 are 8 textures + 1 AS. The primary target already pays this cost; the control's binding set will now occupy those slots. Some are null → fall back to dummy. The cost is one 1x1 dummy texture per fallback slot, already created by `FReSTIRPass::Initialize` (header line 224-225 documents `DummyReservoir` + `DummyGuide`).
  6. **Per-register rule (v200)**: HLSL puts each constant-buffer ARRAY element on its own 16-byte register. The shader-side additions are NOT cbuffer arrays — they are texture declarations. The rule does NOT apply. Confirmed: the primary's same declarations are correct.
  7. **`FBindingLayoutBuilder` `Add*` NOT `Set*` gotcha** (AGENTS.md / v197): this rule is for the C++ binding layout builder, not the shader. The shader uses HLSL `register(...)` syntax. Not relevant.

## Pre-patch verification commands (parent will run, but the planner pre-staged)
```bash
# Verify both shader copies exist (already done this turn)
ls -la Engine/Source/Runtime/Test/TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl
ls -la Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl

# Verify the C++ caller has the TempDesc struct fields (already done this turn — see header lines 102-152)
grep -E "CurrentReservoir2|HistoryReservoir2|WorldPosTexture|MaterialTexture|PrevWorldPosTexture|PrevMaterialTexture" Engine/Source/Runtime/Public/Renderer/PostProcess/FReSTIRPass.h

# Verify the marshaller's ternaries (already done this turn — FReSTIRPass.cpp:606/609/616-619)
grep -nE "DummyReservoir|DummyGuide" Engine/Source/Runtime/Private/Renderer/PostProcess/FReSTIRPass.cpp
```

## Cycle scorecard (pre-edit baseline — recorded by the tester, NOT inherited)

Independent counts at start of this cycle (file-tool reads, NOT inherited from tick-549):

- `search_files pattern="register\(t[0-9]\)" path=TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl` → 8 hits (t0..t7)
- `search_files pattern="register\(t1[0-6]\)" path=TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl` → 0 hits
- `search_files pattern="register\(u0\)|register\(u1\)" path=TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl` → 2 hits (both in DEFAULT space)
- `search_files pattern="space1" path=TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl` → 0 hits
- `search_files pattern="TempDesc\.(Current|History)Reservoir2|WorldPosTexture|MaterialTexture|PrevWorldPosTexture|PrevMaterialTexture" path=TestCornellBoxGI.cpp` → 0 hits

POST-edit expected:
- t0..t9: 10 hits (8 unchanged + 2 new at t8/t9 — but t8/t9 are `register\(t[0-9]\)` per the pattern, so this would catch them)
- t10..t16: 7 hits (all new)
- u0..u3 in space1: 4 hits
- space1 in this file: 4 hits
- TempDesc assignments: 6 hits