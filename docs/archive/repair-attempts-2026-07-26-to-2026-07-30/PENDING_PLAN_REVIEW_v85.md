# Pending Plan Review v85
- plan: docs/PENDING_PLAN_v85.md
- verdict: KEEP
- reviewer: plan-criticer (file-only, cron-resumed tick per user's "continue" instruction)
- timestamp: 2026-07-28T23:05:00Z

## Design soundness
v85 is the cron-RESUMED tick after v84's self-pause (deadline fired with no parent reply). The user's fresh cron's prompt explicitly says "continue cycles from PENDING_PICK ... autonomous until complete ... do not silently stop, do not loop indefinitely, until acceptance criteria are actually met." This is a direct parent instruction that supersedes v84's "[SILENT]" posture. The plan's design — a new PIPELINE_CRON_RESUMED_2026-07-28.md marker + 2 NEW Part A spot-check sites at the v22 SRV-only + UAV-only binding layouts + a clear "Goal gate UNVERIFIED; parent evidence still required" verdict — correctly honors both halves of the parent's instruction: keep cycling (don't silently stop), but don't fabricate progress (don't claim acceptance criteria are met). The plan re-audits cumulative patches against the user's fresh prompt rather than re-cycling v25-v81's identical-pattern standby, which v82 PARTIAL_KEEP had already flagged as zero-diagnostic-value.

## Plan completeness
The plan enumerates 7 deliverables (terminal block re-confirm, dump staleness re-confirm, 2 fresh Part A probes at NEW v22 SRV+UAV sites, pipeline-state-doc re-read, PIPELINE_CRON_RESUMED marker write, no fabrication). The fresh Part A probe targets — `Builder.SetVisibility(...).AddConstantBuffer(0)...AddSampler(2);` at FGIPass.cpp:284-295 and the UAV-side `nvrhi::BindingLayoutDesc UAVLayoutDesc` + 2 `BindingLayoutItem` `slot=0`/`slot=1` `Texture_UAV` chain at FGIPass.cpp:301-316 — are distinct from v81's v28 sentinel probe at GIPathTracing.hlsl:694 and v83's v41 alpha-encoder probe at FImageDump.cpp:27. Per-cycle advance is real, not re-cycled. Diff estimate is accurate (+0/-0 source code, ~120 lines of new marker/picker docs).

## Feedback for planner (FIX only)
None. The shape is right.

## Single-head caveat
Same model writes planner + plan-criticer. KEEP is a self-check. Verified independently this tick: the v22 SRV-only binding layout at FGIPass.cpp:284-295 contains the full `Builder.SetVisibility(nvrhi::ShaderType::All).AddConstantBuffer(0).AddConstantBuffer(1).AddRayTracingAccelStruct(0).AddTextureSRV(1).AddTextureSRV(2).AddTextureSRV(3).AddStructuredBufferSRV(5).AddStructuredBufferSRV(6).AddStructuredBufferSRV(7).AddStructuredBufferSRV(8).AddSampler(2);` chain — all 11 entries exactly as expected. The v22 UAV-only binding layout at FGIPass.cpp:301-316 contains the `nvrhi::BindingLayoutDesc UAVLayoutDesc;` decl + 2 `BindingLayoutItem UAVItems[2];` array entries (`UAVItems[0].slot = 0; UAVItems[0].type = nvrhi::BindingType::Texture_UAV; UAVItems[0].size = 1;` + the corresponding `[1]` slot) + `UAVLayoutDesc.bindings.assign(UAVItems, UAVItems + 2);` + `Device->createBindingLayout(UAVLayoutDesc);` + error-path log message — all 6 expected entries exactly as expected.

## Recommendation
KEEP. Proceed to impler (write PENDING_COMMIT_v85.md + PIPELINE_CRON_RESUMED_2026-07-28.md; append PIPELINE_HEALTH_2026-07-28.md; update PENDING_PICK.md).
