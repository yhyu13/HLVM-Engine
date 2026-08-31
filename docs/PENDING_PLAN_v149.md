# Pending Plan v149
- task: TestReSTIR_GI_Temporal GBuffer SRV binding fix
- source: no bundle — direct edit
- approach: Re-run the unresolved v148 cycle in a terminal-enabled environment. Do not infer a fix from stale artifacts: build the Debug target, inspect current shader reflection and FGIPass bindings, execute fresh HLVM_PT_DEBUG_MODE=20 and normal display runs with HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8, inspect newest PNGs using vision and numpy, run the validator on only the newest dump group, and scan fresh logs. Apply a minimal root-cause-supported source fix only if execution identifies one, then repeat all acceptance checks.
- diff_estimate: +30 / -30 lines
- skip_plan_review: no
- test_strategy: Build Debug target; run mode 20 and normal display; require varying non-zero GBufferMaterial, validator pass on newest group, no Vulkan VUID/ERROR/command-list errors, recognizable sane-exposure Sponza, and successful mode-20 output.
- risks: Terminal security approval may continue to block execution; stale shader blobs, descriptor offsets, SRV/UAV interaction, or mismatched texture handles remain possible causes. No acceptance claim is valid without fresh runtime evidence.

## Superseded (2026-09-07, see docs/PIPELINE_HEALTH_2026-09-07.md)
- The fresh 2026-08-05 15:42 log (4 days newer than the v25 diagnostic source) shows gi_raw R[0,3.26] G[0,3.31] B[0,3.36] mean=0.69 std=0.78 — non-uniform, varying. The GBuffer SRV binding fix IS WORKING. The PICK card "TestReSTIR_GI_Temporal GBuffer SRV binding fix" is closed (marked [x] in PENDING_PICK.md).
- The actual remaining defects are downstream: ReSTIR reservoir M=0 (new card in PENDING_PICK.md). A new plan v150 addresses that card.
- This v149 plan is left in place for state-machine audit trail; its `task` is closed and the next cycle's work is governed by PENDING_PLAN_v150.md.
