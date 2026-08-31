# Pending Plan v141
- task: Fix TestReSTIR_GI_Temporal GBuffer SRV binding
- source: no bundle — direct edit
- approach: Set the FGIPass SRV layout's Vulkan binding offsets explicitly to zero. FBindingLayoutBuilder already stores ShaderMake-shifted slots, so NVRHI defaults otherwise shift b/s descriptors twice; the independently-created UAV layout already follows the zero-offset precedent.
- diff_estimate: +6 / -1 lines
- skip_plan_review: no
- test_strategy: Build and run TestReSTIR_GI_Temporal with Debug, then run the required dump/debug modes, scan fresh logs for Vulkan and command-list errors, run the structural validator against only the newest dump group, and visually/statistically inspect the display output.
- risks: Existing uncommitted diagnostic changes are extensive; do not modify governance files or conflate their behavior. Runtime execution requires terminal approval on this host.
