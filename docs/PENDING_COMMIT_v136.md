# Pending Commit v136
- plan: docs/PENDING_PLAN_v136.md
- files: Engine/Source/Runtime/Private/Renderer/DeviceManagerVk4_LifeCycle.cpp
- source: docs/PENDING_PLAN_v136.md (no bundle — direct edit)
- target: n/a (uncommitted, on disk in working tree)
- task: Revert v132 createValidationLayer call to unblock the build link failure
- verify: `./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug` (terminal required)
- skip_impl_review: no (test-target build file; reviewer should verify patches v131+v135 still intact + v132 cleanly reverted)
- produces_test_files: no
- notes: The change is at `Engine/Source/Runtime/Private/Renderer/DeviceManagerVk4_LifeCycle.cpp:88-97`. Both the CreateDevice (was line 88) and destructor (was line 163) now use `m_ValidationLayer = nullptr;`. v131 patches (FGIPass.cpp:557-562, 675; GIPathTracing.hlsl:685-687, 712-714) and v135 patches are unchanged. v133+v134 cmake flags are unchanged (NVRHI_WITH_VALIDATION=ON, validation TUs in add_library). Behavioral verification requires terminal+vision.

## Plan Deviations (impler fills this in if it deviated from the plan)

None — impler followed the plan exactly. The revert was applied at line 88 (CreateDevice) and the destructor comment block was also updated to reflect v136. Indentation preserved (tabs throughout). The v132 reference comment in the destructor at line 169 was updated to v136 to keep the file's edit-history self-documenting.

---

**Per `six-role-pipeline §Impler deviation policy`, the impler does NOT re-plan inline. The plan's "Risks" section covered the indentation concern (verified tabs preserved).**