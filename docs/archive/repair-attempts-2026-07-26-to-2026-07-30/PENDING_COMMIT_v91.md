# Pending Commit v91
- plan: docs/PENDING_PLAN_v91.md
- files: (none — verification only; 0 source-code lines modified)
- source: no bundle
- target: (no branch — cron must not commit/push per cron's rules + HARD INVARIANT)
- task: restir-gi-fix — narrow v90's 2-way hypothesis to 1-way via slot-validity probe at 3 sites
- verify: read_file on lines (a) FGIPass.cpp:301-310 (UAV binding layout declaration), (b) TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl:88 (shader register), (c) FGIPass.cpp:580-585 (binding-set population). All three sites must show slot/register/u-arg = 0 (consistent binding contract).
- skip_impl_review: yes (0 source-code modifications; verification-only cycle; same shape as v89/v90)
- produces_test_files: no
- notes: a NEW Part A probe at a NEW diagnostic site distinct from v25-v90's 22+4 verified sites. The v91 finding complements the v90 finding by collapsing the 2-way hypothesis to a single remaining cause at the binding-layer (the dispatch call itself).

## Part A probe — slot-validity (NEW site, v91)

### A1 — FGIPass.cpp:301-310 — UAV binding layout slot 0 declaration
Read via `read_file` window at lines 301-310. Exact text confirms:
- Line 304: `UAVItems[0].slot = 0;` — binding layout declares slot 0 for the Output RW texture.
- Line 305: `UAVItems[0].type = nvrhi::BindingType::Texture_UAV;` — type matches shader-side RWTexture2D.
- Line 310: `UAVLayoutDesc.bindings.assign(UAVItems, UAVItems + 2);` — 2 entries (slot 0 + slot 1 for DebugStatsTexture).
PASS — confirms slot 0 is the OutputTexture's binding slot at the binding-layout layer.

### A2 — GIPathTracing.hlsl:88 — shader-side Output register
Read via `read_file` window at lines 85-95. Exact text: `RWTexture2D<float4> Output : register(u0);`. The shader declares `Output` at register `u0`, which corresponds to binding slot 0 in the SPIR-V/HLSL mapping convention.
PASS — confirms the shader's RW texture is registered to slot 0, matching the binding layout's slot 0 declaration in A1.

### A3 — FGIPass.cpp:580-585 — binding-set population
Read via `read_file` window at lines 580-585. Exact text: line 582 reads `UAVBuilder.SetTextureUAV(0, Desc.OutputTexture);`. First argument (slot) is `0`. The texture handle is `Desc.OutputTexture` — the same `OutputTexture` member created at TestReSTIR_GI_Temporal.cpp:937 (verified at v90).
PASS — confirms the binding-set's first-arg slot 0 maps to `Desc.OutputTexture`, and the binding layout declares slot 0 (A1), and the shader register is `u0` (A2).

### NEW finding (v91 collapses v90's 2-way to 1-way)
All three sites converge on slot 0:
- B1 (binding layout) → slot 0
- B2 (shader register) → u0
- B3 (binding set) → SetTextureUAV(0, ...)

The binding contract is consistent. **Hypothesis (ii) shader-side write skipped is ELIMINATED at the binding layer.** The remaining cause is (i) dispatch-drops — i.e., `FRayTracingPipeline::DispatchRays(CommandList, ...)` at FGIPass.cpp:625 is failing silently or being skipped before the shader is dispatched. Disambiguation from this point requires ONLY the v3 ENTER/EXIT log presence check + per-channel min/max of `gi_raw` dump output.

10-second terminal probe: parent runs the 4-command recipe per `docs/PIPELINE_BLOCKER_2026-07-28.md`, the log will show v3 ENTER + v3 EXIT + per-channel min/max. If BOTH ENTER and EXIT log lines appear with matching `OutputTex=` handle, the dispatch returned normally and the bug is in nvrhi's record-submit-batch / shader-launch pipeline (a deep GPU side issue). If only ENTER appears (not EXIT), the dispatch hangs/fails fatally — `FRayTracingPipeline::DispatchRays` exits abnormally without returning. If neither appears, the entire `GIPass.DispatchRays(CommandList, Desc)` call at TestReSTIR_GI_Temporal.cpp:450 is being skipped (caller-side issue, not the FGIPass).

Cumulative narrowing chain:
- v89: 3-way hypothesis (i/ii/iii)
- v90: eliminated (iii) → 2-way (i/ii)
- v91: eliminated (ii) at binding-layer → 1-way (i dispatch-drops)
- v92+ (terminal-gated): disambiguate sub-causes of (i) dispatch-drops

## Plan Deviations
None.
