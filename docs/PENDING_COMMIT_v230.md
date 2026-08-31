# Pending Commit v230
- plan: docs/PENDING_PLAN_v230.md
- files: Engine/Source/Runtime/Test/TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl, Engine/Source/Runtime/Test/TestCornellBoxGI.cpp
- source: no bundle
- target: branch (parent-owned)
- task: Card N — sync TestCornellBoxGI's temporal shader + TempDesc to FReSTIRPass's shared TemporalLayoutSRV (16 SRVs + RT AS) and TemporalLayoutUAV (4 UAVs in space1). Closes the layout-vs-consumer divergence the v203 cycle identified.
- verify: `cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine && ./Build.sh --Config=Debug --Target=TestCornellBoxGI --Rebuild && cd Engine/Source/Runtime/Binary/Debug && ./TestCornellBoxGI && grep -E "VUID|ERROR" TestCornellBoxGI.log`
- skip_impl_review: yes
- produces_test_files: no
- notes: This patch is a binding-contract reconciliation, NOT a visual-quality fix. After the patch:
  1. Cornell's `ReSTIR_Temporal_cs.hlsl` SPIR-V declares t0..t16 + 4 UAVs in space1, matching the shared layouts at FReSTIRPass.cpp:243-262 and :295-300.
  2. Cornell's `TempDesc` populates all 6 missing fields (`CurrentReservoir2`, `HistoryReservoir2`, `WorldPosTexture`, `MaterialTexture`, `PrevWorldPosTexture`, `PrevMaterialTexture`) with `nullptr`. `FReSTIRPass::DispatchTemporal`'s ternaries at lines 606/609/616-619 fall back to `DummyReservoir`/`DummyGuide` for nulls, so the binding set is still populated and Vulkan validation stays green.
  3. Cornell's temporal pass becomes data-starved on the 6 new slots — the temporal reuse algorithm will read from 1x1 dummy textures for the Phase-2/3/4 surfaces, producing noisy but not crashing output. Acceptable because Cornell is a **known-good control** whose value is its provenance as unmodified, not as a best-looking render.
  4. The primary target's `TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl` was NOT edited. Verify by re-running `search_files pattern="register\(t1[0-6]\)" path=TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl` → 7 hits (unchanged from baseline).

The v182 dual-copy hazard does NOT apply: the shader has only 2 copies (`TestCornellBoxGI_Data/` + `TestReSTIR_GI_Temporal_Data/`), no `Private/Renderer/Shader/` source-of-truth exists, and editing both copies would BREAK the primary target (which is the correct version). The control's copy is the diverged one; only it was edited.

Diff stats:
- Shader: +28 / -2
- C++: +15 comment / +6 functional / -0
- Net functional lines: +34 / -2

Pre-edit counts (from this turn's independent file reads, NOT inherited):
- `register(t[0-9])` in control: 8 hits
- `register(t1[0-6])` in control: 0 hits
- `register(u0)` or `register(u1)` in control: 2 hits
- `space1` in control: 0 hits
- `TempDesc.(Current|History)Reservoir2|WorldPosTexture|MaterialTexture|PrevWorldPosTexture|PrevMaterialTexture` in TestCornellBoxGI.cpp: 0 hits

Post-edit counts (from this turn's edits):
- `register(t[0-9])` in control: 10 hits (t0..t9)
- `register(t1[0-6])` in control: 7 hits (t10..t16)
- `register(u0..u3)` in control: 4 hits
- `space1` in control: 4 hits
- `TempDesc.(Current|History)Reservoir2|WorldPosTexture|MaterialTexture|PrevWorldPosTexture|PrevMaterialTexture` in TestCornellBoxGI.cpp: 6 hits

## Plan Deviations (impler fills this in if it deviated)

None. The patch followed the plan byte-for-byte. The plan-criticer's 3 corrections (C++ side, diff estimate, dual-copy negative) were all incorporated in the plan revision and the impler applied them as specified.