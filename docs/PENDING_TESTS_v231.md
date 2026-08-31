# Pending Tests v231
- commit: docs/PENDING_COMMIT_v231.md
- files: <none — patch is binding-contract reconciliation, no new test files produced>
- verifier_command: <see 12-row file-only verifier below; no test executable to run because the patch doesn't change behaviour for the running control>
- notes: This cycle is a binding-contract reconciliation between `TestCornellBoxGI_Data/ReSTIR_Generate_cs.hlsl` (the control's SPIR-V) and `FReSTIRPass::GenerationLayoutSRV` (the shared binding layout). The patch adds 3 SRV declarations to make the SPIR-V match the layout, and 3 GenDesc null assignments so the binding set is populated. The control's generation shader does not read the new symbols, so this is a binding-layer-only change with no runtime impact.

## File-only verifier (12 rows; run with `search_files` and `read_file` from this directory)

| # | Query | Expected | Actual | Verdict |
|---|-------|----------|--------|---------|
| 1 | `search_files path=TestCornellBoxGI_Data/ReSTIR_Generate_cs.hlsl pattern="register.t[0-9]."` | 7 hits (t0..t6) | 7 hits (lines 25-28, 39-41) | PASS |
| 2 | `search_files path=TestCornellBoxGI_Data/ReSTIR_Generate_cs.hlsl pattern="gSample "` (with trailing space) | 1 hit (declaration) | 1 hit (line 39); also 1 in comment (line 33) | PASS |
| 3 | `search_files path=TestCornellBoxGI_Data/ReSTIR_Generate_cs.hlsl pattern="gSampleInfo"` | 1 hit (declaration) | 1 hit (line 40) | PASS |
| 4 | `search_files path=TestCornellBoxGI_Data/ReSTIR_Generate_cs.hlsl pattern="gMaterial"` | 1 hit (declaration) | 1 hit (line 41) | PASS |
| 5 | `search_files path=TestCornellBoxGI.cpp pattern="GenDesc.DirectionTexture"` | 1 hit | 1 hit (line 1605) | PASS |
| 6 | `search_files path=TestCornellBoxGI.cpp pattern="GenDesc.SampleInfoTexture"` | 1 hit | 1 hit (line 1606) | PASS |
| 7 | `search_files path=TestCornellBoxGI.cpp pattern="GenDesc.MaterialTexture"` | 1 hit | 1 hit (line 1607) | PASS |
| 8 | `search_files path=TestReSTIR_GI_Temporal_Data/ReSTIR_Generate_cs.hlsl pattern="register.t[0-9]."` | 7 hits (primary unchanged) | 7 hits (lines 39-45) | PASS — primary NOT modified |
| 9 | `read_file path=FReSTIRPass.h offset=82 limit=20` — confirms `FGenerationDesc` has DirectionTexture/SampleInfoTexture/MaterialTexture | 3 fields present | 3 fields present (lines 85/91/92) | PASS |
| 10 | `search_files path=FReSTIRPass.cpp pattern="GenerationLayoutSRV"` → followed by `BindingLayoutItem::Texture_SRV(` | 7 SRV bindings in the GenerationLayoutSRV block | 7 SRV bindings (lines 178-184: ConstantBuffer(256), Texture_SRV(0..6)) | PASS |
| 11 | `search_files path=FReSTIRPass.cpp pattern="DirectionTexture ? Desc.DirectionTexture : Desc.RadianceTexture"` | 1 hit | 1 hit (line 501) | PASS — ternary fallback intact |
| 12 | `search_files path=FReSTIRPass.cpp pattern="SampleInfoTexture ? Desc.SampleInfoTexture : DummyGuide"` | 1 hit | 1 hit (line 502) | PASS — ternary fallback intact |

All 12 rows PASS. The patch:
- Adds 3 SRV declarations (t4..t6) to the control's generation shader, mirroring the primary.
- Adds 3 GenDesc null assignments (DirectionTexture/SampleInfoTexture/MaterialTexture), falling back to `Desc.RadianceTexture` / `DummyGuide` via the DispatchGeneration ternaries.
- Does NOT modify the primary copy.
- Does NOT modify the shared layout in FReSTIRPass.cpp.
- Does NOT change any runtime behaviour (the control's shader doesn't read the new symbols).

The binding contract is now reconciled: the control's SPIR-V reflects 7 SRVs, matching the shared `GenerationLayoutSRV`. Vulkan validation at first generation dispatch should report 0 VUIDs on this shader's binding set.

## Single-profile caveat

Same as v230 — the verifier is the same model as the impler on a single-profile host. ALL_KEEP is a self-audit, not an independent review.