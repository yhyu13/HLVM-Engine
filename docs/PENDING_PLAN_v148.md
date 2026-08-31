# Pending Plan v148
- task: TestReSTIR_GI_Temporal GBuffer SRV binding fix
- source: no bundle — direct edit
- approach: Resume the unresolved v147 cycle with terminal-enabled execution. Inspect the current FGIPass binding/layout and shader reflection, then apply only a root-cause-supported fix for the zero GBuffer SRV reads. Build and run fresh mode-20 and display captures, inspect newest PNGs with per-pixel statistics/vision, validate the newest dump group, and scan logs for Vulkan and command-list errors.
- diff_estimate: +30 / -30 lines
- skip_plan_review: no
- test_strategy: Build Debug target; run HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 with HLVM_PT_DEBUG_MODE=20 and normal display mode; require varying non-zero mode-20 GBufferMaterial, validator pass on newest dump group, no VUID/ERROR/command-list errors, recognizable sane-exposure Sponza, and successful mode-20 output.
- risks: Existing file-only cycles cannot establish runtime truth; stale shader blobs, descriptor offsets, SRV/UAV binding interactions, or mismatched texture handles may still be the root cause. No acceptance claim is valid without fresh execution evidence.
