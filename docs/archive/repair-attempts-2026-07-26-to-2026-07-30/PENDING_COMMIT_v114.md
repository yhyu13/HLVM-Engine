# Pending Commit v114
- plan: docs/PENDING_PLAN_v114.md
- files: Engine/Source/Runtime/Public/Renderer/RayTracing/FRayTracingPipeline.h, Engine/Source/Runtime/Private/Renderer/RayTracing/FRayTracingPipeline.cpp, Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp, Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl, Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl
- source: direct edit informed by docs/restir-gi-fix-v101.patch
- target: current working tree (no commit/branch operation permitted)
- task: complete and align the split SRV/UAV ray-tracing descriptor contract
- verify: ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal
- skip_impl_review: no
- produces_test_files: no
- notes: Added the UAV layout as the second ordinary global binding layout, aligned its raw NVRHI slots with FBindingSetBuilder's URegShift (384/385), moved both shader UAV declarations to space1, and clear the additional-layout vector during shutdown. Static source reads confirm the intended edits. Build was attempted but tirith returned pending_approval, so compilation and GPU acceptance are UNVERIFIED.

## Plan Deviations
None.
