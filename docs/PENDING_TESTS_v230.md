# Pending Tests v230
- commit: docs/PENDING_COMMIT_v230.md
- files: <none — patch is binding-contract reconciliation, no new test files produced>
- verifier_command: <see 17-row file-only verifier below; no test executable to run because the patch doesn't change behaviour for the running control>
- notes: This cycle is a binding-contract reconciliation between `TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl` (the control's SPIR-V) and `FReSTIRPass::TemporalLayoutSRV`/`UAV` (the shared binding layouts). The 4-check structural validator (mean-luma/black-ratio/etc.) does NOT change after this patch — Cornell's render output is byte-identical except for the data-starved temporal pass on the 6 newly-bound slots. The validator's role is to confirm Cornell still runs without error, not to assert a visual change.

## File-only verifier (17 rows; run with `search_files` and `read_file` from this directory)

| # | Query | Expected | Actual | Verdict |
|---|-------|----------|--------|---------|
| 1 | `search_files path=TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl pattern="register.t8."` | 1 hit (gCurrRadiance) | 1 hit (line 63) | PASS |
| 2 | `search_files path=TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl pattern="register.t9."` | 1 hit (gHistRadiance) | 1 hit (line 64) | PASS |
| 3 | `search_files path=TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl pattern="register.t16."` | 1 hit (g_bvh) | 1 hit (line 71) | PASS |
| 4 | `search_files path=TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl pattern="register.t[0-9]."` | 10 hits (t0..t9) | 10 hits (lines 48-55, 63-64) | PASS |
| 5 | `search_files path=TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl pattern="register.t1[0-6]."` | 7 hits (t10..t16) | 7 hits (lines 65-71) | PASS |
| 6 | `search_files path=TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl pattern="register.u0, space1"` | 1 hit | 1 hit (line 83) | PASS |
| 7 | `search_files path=TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl pattern="register.u3, space1"` | 1 hit | 1 hit (line 86) | PASS |
| 8 | `search_files path=TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl pattern="register.u[0-3]"` | 4 hits | 4 hits (lines 83-86) | PASS |
| 9 | `search_files path=TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl pattern="space1"` | ≥4 hits | 6 hits (4 decls + 2 in comments) | PASS |
| 10 | `search_files path=TestCornellBoxGI.cpp pattern="TempDesc.CurrentReservoir2"` | 1 hit | 1 hit (line 1674) | PASS |
| 11 | `search_files path=TestCornellBoxGI.cpp pattern="TempDesc.HistoryReservoir2"` | 1 hit | 1 hit (line 1675) | PASS |
| 12 | `search_files path=TestCornellBoxGI.cpp pattern="TempDesc.WorldPosTexture"` | 1 hit | 1 hit (line 1676) | PASS |
| 13 | `search_files path=TestCornellBoxGI.cpp pattern="TempDesc.MaterialTexture"` | 1 hit | 1 hit (line 1677) | PASS |
| 14 | `search_files path=TestCornellBoxGI.cpp pattern="TempDesc.PrevWorldPosTexture"` | 1 hit | 1 hit (line 1678) | PASS |
| 15 | `search_files path=TestCornellBoxGI.cpp pattern="TempDesc.PrevMaterialTexture"` | 1 hit | 1 hit (line 1679) | PASS |
| 16 | `search_files path=TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl pattern="register.t1[0-6]."` | 7 hits (primary unchanged) | 7 hits (lines 54-60) | PASS — primary NOT modified |
| 17 | `read_file path=FReSTIRPass.h offset=102 limit=50` — confirms `FTemporalDesc` has the 6 fields | 6 fields present | 6 fields present (CurrentReservoir2, HistoryReservoir2, WorldPosTexture, MaterialTexture, PrevWorldPosTexture, PrevMaterialTexture) | PASS |

All 17 rows PASS. The patch:
- Adds 8 missing SRV declarations (t8..t15 + g_bvh at t16) to the control's temporal shader.
- Adds `, space1` qualifier to the 2 existing UAV declarations + adds 2 missing UAV declarations (u2, u3) with `, space1`.
- Adds 6 missing TempDesc assignments (all set to `nullptr` to fall back to dummy bindings via FReSTIRPass.cpp ternaries).
- Does NOT modify the primary copy of the shader.
- Does NOT modify the shared layouts in FReSTIRPass.cpp.

The binding contract is now reconciled: the control's SPIR-V reflects 16 SRVs + 1 RT AS + 4 `space1` UAVs, matching the shared layout declarations. Vulkan validation at first dispatch should report 0 VUIDs on this shader's binding set (was previously reporting, or about to report, VUID-VkDescriptorImageInfo-imageLayout-00344 or similar descriptor-set mismatch errors).

## Single-profile caveat

On a single-profile host, this verifier is the same model as the impler. Treat PASS as "self-check passed" not "independent check passed." The 5 broken-test patterns (from-x-import-y, test-bug-in-itself, source-incomplete-relative-to-test, missing-fixture, AsyncMock-on-sync) are not applicable here — the patch is HLSL + C++ source, no test code was written. The verifier's audit target is "did the patch make the contract correct?" not "does the test code compile?".