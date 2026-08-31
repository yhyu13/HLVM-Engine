# Pending Commit v117
- plan: docs/PENDING_PLAN_v117.md
- files: none (verification-only cycle; no source or test files changed)
- source: current working-tree v114 implementation
- target: current working tree (no commit/branch operation permitted)
- task: execute and audit the v114 split-layout repair against the GPU acceptance contract
- verify: ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal
- skip_impl_review: no
- produces_test_files: no
- notes: The canonical Debug build was attempted unchanged from the project root, but the terminal tool returned `status=pending_approval`, `exit_code=-1`, `pattern_key=tirith:unknown` before compiler output. Therefore no executable, fresh ACCUM=8 run, appended log region, newest dump group, validator result, statistics, or visual evidence exists. No production/test source was edited and no success is claimed. File-based static checks confirm the v114 contract remains present: `FRayTracingPipeline` appends and clears `AdditionalBindingLayouts`; `FGIPass` creates shifted UAV slots with `FBindingLayoutBuilder::URegShift + 0/1` and adds the UAV layout; and both GI shader copies declare `Output` and `DebugStatsTexture` in `space1`.

## Plan Deviations
- **What changed from the plan**: Real build/run verification could not execute.
- **Why**: Terminal authorization returned `pending_approval: tirith:unknown` before launching `Build.sh`.
- **Impact on acceptance criteria**: All six runtime and visual gates remain unverified; completion cannot be claimed.
- **Justification**: This is an external runspace-authorization blocker, not a renderer-design change. Preserving source unchanged avoids speculative GPU edits without fresh evidence.
