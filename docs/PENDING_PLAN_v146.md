# Pending Plan v146
- task: TestReSTIR_GI_Temporal GBuffer SRV binding fix
- source: no bundle — direct edit
- approach: First perform a fresh binding/descriptor bisect rather than assume the prior C++ handles are correct: inspect the generated GI shader reflection and the actual FGIPass binding layout/set, then compare the resource identities and transitions used by GBuffer output versus GI input. Apply one narrow fix at the proven boundary, preserving the existing debug modes. Build and run the Debug target with mode 20, mode 0, accumulation 8, fresh dumps, validator, log scan, and image/statistics inspection.
- diff_estimate: +20 / -20 lines
- skip_plan_review: no
- test_strategy: Role #5 runs the Debug target and fresh HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 mode-20 and mode-0 runs; verifies non-zero varying mode-20 material, validator newest group, no Vulkan/command-list errors, and visual Sponza exposure.
- risks: Existing diagnostic may be stale; shader blobs may not regenerate; descriptor register/offset mismatch, stale texture handles, or SRV/UAV state ordering may remain. Do not claim success from direct staging dumps alone; mode 20 must read the same resources through the GI shader.
