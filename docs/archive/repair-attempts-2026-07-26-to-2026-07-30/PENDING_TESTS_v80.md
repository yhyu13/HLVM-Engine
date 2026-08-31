# Pending Tests v80
- commit: docs/PENDING_COMMIT_v80.md
- task: structural standby tick verification (62nd consecutive file-only tick v25-v80)

## Part A — static probes (file-only, no shell required)

| # | Probe | Expected | Status |
|---|-------|----------|--------|
| A1 | v22 UAVBindingLayout at FGIPass.cpp:183 (init) | `m_UAVBindingLayout = nullptr;` or equivalent | PENDING |
| A2 | v22 UAVBindingLayout at FGIPass.cpp:311 (create) | `m_UAVBindingLayout = ...;` with SRV-vs-UAV split | PENDING |
| A3 | v22 UAVBindingLayout at FGIPass.cpp:612 (use) | `State.addBindingSet(m_UAVBindingLayout.Get());` or equivalent | PENDING |
| A4 | v41 std::clamp alpha-encoder at Private/Image/FImageDump.cpp:27 | `std::clamp(rgbaData[i*4+3] * 255.0f, 0.0f, 255.0f)` | PENDING |
| A5 | v17 case 7u at GIPathTracing.hlsl:604 in BOTH copies | `case 7u:` present in Private/data-dir | PENDING |
| A6 | v28 alpha-sentinel at GIPathTracing.hlsl:694 in BOTH copies | `Output[pixel].w = max(Output[pixel].w, 0.99994f);` | PENDING |
| A7 | bug-088 executeCommandList at TestReSTIR_GI_Temporal.cpp:691 | `NvrhiDevice->executeCommandList(CommandList);` | PENDING |
| A8 | v22 2-overload DispatchRays at FRayTracingPipeline.cpp:381 | `State.addBindingSet(m_SRVBindingSet.Get());` + `State.addBindingSet(m_UAVBindingSet.Get());` | PENDING |

## Part B — runtime probes (parent-driven; terminal blocked by tirith)

| # | Probe | Expected | Status |
|---|-------|----------|--------|
| B1 | `bash fresh-evidence-scan.sh` exit code | 0 (parent rebuilt recently with all patches) | PENDING |
| B2 | `validate_restir_gi.py` on newest dump group | 3/3 PASS | PENDING |
| B3 | `display_frame8.png` vision check | recognizable non-uniform Sponza geometry | PENDING |
| B4 | `grep stderr.log VUID-VkDescriptorImageInfo-imageLayout-00344` count | 0 (v22 binding-layout-split zero-VUID check) | PENDING |
| B5 | v12 cerr lines in stderr.log (`[RGI] Render() entry:` x8 + `[RGI] FGIPass::DispatchRays() entry:` x8) | 16 lines total | PENDING |
| B6 | v38 cerr DebugMode effective line | shows `DebugMode effective=N cvar=M env_var=S/null Params5[0]=F` | PENDING |
| B7 | v28 alpha-sentinel inspection (`display_frame8.png` alpha channel) | saturated 254-255 OR zero (binary dispatch signal) | PENDING |
| B8 | `cat TestReSTIR_GI_Temporal.log` gi_raw value | non-zero (was [0,0,0] in stale runs) | PENDING |

## Decision (per evidence shape)

If parent supplies terminal evidence (rebuild + stderr.log + dump + validator + vision), v80 routes to whichever of v17/v13a/v32/v33/v42 best matches the evidence shape. Otherwise v80 transitions to [SILENT] per v62 "[SILENT] transition" guidance.
