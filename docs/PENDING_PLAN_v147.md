# Pending Plan v147
- task: TestReSTIR_GI_Temporal GBuffer SRV binding fix
- source: no bundle — direct edit
- approach: Resume the blocked runtime bisect from v146. Inspect current shader reflection and FGIPass resource bindings, then make only a root-cause-supported binding/transition fix. Build and execute fresh mode-20 and display runs, inspect newest PNGs with numpy/vision, validate the newest dump group, and scan logs for Vulkan VUID/ERROR and command-list failures.
- diff_estimate: +20 / -20 lines
- skip_plan_review: no
- test_strategy: Build Debug target; run HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 with HLVM_PT_DEBUG_MODE=20 and normal display mode; require non-zero varying GBufferMaterial from mode 20, validator pass on newest dump group, no VUID/ERROR/command-list errors, and recognizable sane-exposure Sponza.
- risks: Execution may remain blocked by the host security gate; stale shader blobs, mismatched handles, descriptor offsets, or SRV/UAV ordering may produce zero reads. Direct staging dumps are not sufficient evidence.
