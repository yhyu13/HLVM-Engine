# Pending Commit v137
- plan: docs/PENDING_PLAN_v137.md
- files: Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp
- source: docs/PENDING_PLAN_v137.md (no bundle — direct edit)
- target: n/a (uncommitted, on disk in working tree)
- task: Add `setBindingOffsets(0,0,0,0)` to FGIPass's UAV-only binding layout (descriptor-slot double-add bug fix)
- verify: `./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug` (terminal required)
- skip_impl_review: no (test-target build file; reviewer should verify patches v131+v135+v136 still intact + v137 applied correctly)
- produces_test_files: no
- notes: The change is at `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:301-318`. Added a 4-line `nvrhi::VulkanBindingOffsets UAVOffsets` block + a 1-line `setBindingOffsets(UAVOffsets)` call BEFORE the items are added. Items at slots 384/385 unchanged. v131 patches (FGIPass.cpp:557-562, 675) and v135 patches unchanged. v136 patches unchanged (DeviceManagerVk4_LifeCycle.cpp:88, 163). Behavioral verification requires terminal+vision.

## Plan Deviations (impler fills this in if it deviated from the plan)

Minor deviation from plan: added the explicit `VulkanBindingOffsets` struct form rather than the 4-arg overload, because the plan-criticer's feedback noted the 4-arg overload only works with `FBindingLayoutBuilder`, not raw `nvrhi::BindingLayoutDesc`. The struct form matches the FReSTIRPass precedent (FReSTIRPass.cpp:161-163, 186-188, 207-208). This is a minor clarification, not a substantive deviation — the fix is the same effect (bindingOffsets=0,0,0,0).

---

**Per `six-role-pipeline §Impler deviation policy`, the impler does NOT re-plan inline. The plan's "Risks" section covered the binding-offsets form ambiguity (resolved per plan-criticer's feedback to use the struct form).**