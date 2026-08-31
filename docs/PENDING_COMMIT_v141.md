# Pending Commit v141
- plan: docs/PENDING_PLAN_v141.md
- files: Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp
- source: no bundle
- target: current working tree (no commit permitted)
- task: Explicitly zero Vulkan binding offsets for the FGIPass SRV layout.
- verify: ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
- skip_impl_review: no
- produces_test_files: no
- notes: Runtime verification was attempted but terminal execution is pending approval under tirith.

## Implementation
Added `Builder.SetBindingOffsets(0, 0, 0, 0)` before populating the SRV layout. This prevents NVRHI from adding its default b/s/u offsets to slots already shifted by FBindingLayoutBuilder and mirrors the existing UAV layout.

## Plan Deviations
None.
