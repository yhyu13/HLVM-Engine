# Pending Tests v81
- commit: docs/PENDING_COMMIT_v81.md
- task: structural standby tick verification (63rd consecutive file-only tick v25-v81)

## Part A — static probes (file-only, no shell required)

| #  | Probe | Expected                                                                                    | Status |
|----|-------|---------------------------------------------------------------------------------------------|--------|
| A1 | v28 alpha-sentinel at GIPathTracing.hlsl Private:694 | `Output[pixel].w = max(Output[pixel].w, 0.99994f);` | **PASS** (fresh v81 probe) |
| A2 | v28 alpha-sentinel at GIPathTracing.hlsl Data:694    | `Output[pixel].w = max(Output[pixel].w, 0.99994f);` | **PASS** (fresh v81 probe) |
| A3 | v22 binding-layout UAVBindingLayout at FGIPass.h:106 | `nvrhi::BindingLayoutHandle UAVBindingLayout; // v22 split` | PASS (v79 cross-tick) |
| A4 | v22 binding-layout State.addBindingSet x2 at FRayTracingPipeline.cpp:357/361 | both `SRVBindingSet` and `UAVBindingSet` calls present | PASS (v79 cross-tick) |
| A5 | v22 binding-layout 2-overload DispatchRays at FRayTracingPipeline.cpp:345/357/361/375/381 | both overloads + `addBindingSet(m_SRVBindingSet.Get())` + `addBindingSet(m_UAVBindingSet.Get())` present | PASS (v79 cross-tick) |
| A6 | bug-088 executeCommandList at TestReSTIR_GI_Temporal.cpp:691 | `NvrhiDevice->executeCommandList(CommandList);` | PASS (v79 cross-tick) |
| A7 | v41 std::clamp alpha-encoder at Private/Image/FImageDump.cpp:27 | `std::clamp(rgbaData[i*4+3] * 255.0f, 0.0f, 255.0f)` | PASS (v79 cross-tick) |
| A8 | v17 case 7u at GIPathTracing.hlsl Private:604 + Data:604 | `case 7u:` present in BOTH copies | PASS (v79 cross-tick) |
| A9 | v3 spdlog FGIPass::DispatchRays ENTER at FGIPass.cpp:511 | HLVM_LOG LogGI info "FGIPass::DispatchRays ENTER" | PASS (v35 cross-tick) |
| A10| v12 cerr writes default-ON at TestReSTIR_GI_Temporal.cpp:384 + FGIPass.cpp:487 | `std::cerr << "[RGI]..."` x2 | PASS (v35 cross-tick) |
| A11| v32 fresh-evidence-scan.sh helper script present | exists at TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh | PASS (v35 cross-tick) |
| A12| v23 run_rgi_diagnostic.sh + v24 dump_pixelstats.py present | both files exist in TestReSTIR_GI_Temporal_Data/ | PASS (v35 cross-tick) |

## Part B — runtime probes (parent-driven; terminal blocked by tirith this turn)

| #  | Probe | Expected | Status |
|----|-------|----------|--------|
| B1 | `bash fresh-evidence-scan.sh` exit code | 0 (parent rebuilt with all patches) | PENDING |
| B2 | `validate_restir_gi.py` on newest dump group | 4/4 PASS (incl. v37 alpha_sentinel check) | PENDING |
| B3 | `display_frame8.png` vision check | recognizable non-uniform Sponza geometry | PENDING |
| B4 | `grep stderr.log VUID-VkDescriptorImageInfo-imageLayout-00344` count | 0 (v22 binding-layout-split zero-VUID check) | PENDING |
| B5 | v12 cerr lines in stderr.log | 16 lines total (8 Render + 8 DispatchRays) | PENDING |
| B6 | v38 cerr DebugMode effective line | shows `DebugMode effective=N cvar=M env_var=S/null Params5[0]=F` | PENDING |
| B7 | v28 alpha-sentinel `display_frame8.png` alpha channel | saturated 254-255 OR uniform 0 (binary dispatch signal) | PENDING |
| B8 | `cat TestReSTIR_GI_Temporal.log` gi_raw value | non-zero (was [0,0,0] in stale 2026-07-27 run) | PENDING |

## Part C — goal gate (UNVERIFIED — all 6 criteria require parent action from cron's prompt)
- (a) Debug target builds cleanly — UNVERIFIED
- (b) Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — UNVERIFIED
- (c) No command-list-already-open errors — UNVERIFIED
- (d) No Vulkan ERROR/VUID in fresh log — UNVERIFIED
- (e) Validator passes newest dump group — UNVERIFIED
- (f) Display visibly contains recognizable non-uniform Sponza — UNVERIFIED
- No `PIPELINE_GOAL_DONE_<date>.md` written.

## Single-head caveat
Same model writes tester + testing-verifier. Verdicts are self-checks.

## Recommendation
**Mechanical PASS Part A static tests** (12/12 PASS, including fresh v81 A1+A2 v28 alpha-sentinel probe in BOTH Private+Data HLSL copies). **UNVERIFIED Part B + Part C** pending parent terminal access.
