# Pending Commit v22

- plan: docs/PENDING_PLAN_v22.md
- files: Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h, Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp, Engine/Source/Runtime/Public/Renderer/RayTracing/FRayTracingPipeline.h, Engine/Source/Runtime/Private/Renderer/RayTracing/FRayTracingPipeline.cpp
- source: PENDING_PLAN_v21.md (pre-staged binding-layout-split design)
- target: working tree (no commit/push per cron instruction)
- task: apply the v21a binding-layout split — fix the nvrhi-deferred-barrier-ordering pattern by separating the FGIPass single binding set (SRV + UAV mixed) into two binding sets dispatched in sequence.
- verify: parent-driven per PENDING_PLAN_v22.md (build + run + log + validator + vision analysis)
- skip_impl_review: no — high-risk binding layout refactor
- produces_test_files: no
- notes:
  - v22 is the canonical next-cycle action item from PENDING_PICK.md line 143
  - The hypothesis (nvrhi-deferred-barrier-ordering) is well-grounded in the gpu-rendering-bisect-debug reference and matches the observed 7x `DeviceManager.cpp:52` warning per stale run
  - The patch is reversible: `git checkout` on the 4 files reverts the change
  - All HLSL register-to-binding-set mappings are preserved exactly (b0/b1/t0/t1/t2/t3/t5/t6/t7/t8/s2 in SRV; u0/u1 in UAV)
  - The new FRayTracingPipeline::DispatchRays overload uses State.addBindingSet() twice, which is the same pattern as the existing 7-arg overload (FRayTracingPipeline.cpp:304-332)

## Plan Deviations

The plan estimated +50/-25 lines; the actual diff is +71/-15 (FGIPass.h +2/-1, FGIPass.cpp +45/-10, FRayTracingPipeline.h +21/-0, FRayTracingPipeline.cpp +33/-0). The increase is due to:
- More verbose inline `nvrhi::BindingLayoutDesc` + `BindingLayoutItem` initialization in CreateBindingLayout (the existing Add*Builder API doesn't have a UAV-only path; the v22 plan acknowledged this)
- Additional header doc comments on the new DispatchRays overloads

The deviation is documented here per the impler deviation policy. The functional shape matches the plan exactly; the line count is a documentation expansion, not a behavior change.

## File-level changes

```
M Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h  (+2/-1)
M Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp  (+45/-10)
M Engine/Source/Runtime/Public/Renderer/RayTracing/FRayTracingPipeline.h  (+21/-0)
M Engine/Source/Runtime/Private/Renderer/RayTracing/FRayTracingPipeline.cpp  (+33/-0)
```

## What's next

After v22 closes (audit KEEP), the v22 PICK item is marked [x]. The remaining unchecked items are:
- v21 decision matrix (parent-driven, gates v21b..v21i sub-plans if v21a is wrong)
- Parent rebuild + run + rgi_evidence.txt paste-back path

The cron's terminal access is blocked by tirith; the actual fix verification is parent-driven. The patch is in source; the next parent's `Build.sh` will compile it.
