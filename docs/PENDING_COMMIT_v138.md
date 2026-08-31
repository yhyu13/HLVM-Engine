# Pending Commit v138
- plan: docs/PENDING_PLAN_v138.md
- files: Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl
- source: docs/PENDING_PLAN_v138.md (no bundle — direct edit)
- target: n/a (uncommitted, on disk in working tree)
- task: Add `6u` to `bypassEarlyReturn` debug-mode list so mode 6 actually discriminates UAV-bug vs SRV-bug
- verify: `./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug` (terminal required)
- skip_impl_review: no (shader file; reviewer should verify the bypass chain is structurally correct and v137 patch still intact)
- produces_test_files: no
- notes: The change is at `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl:475-485`. Added `|| debugModeEarly == 6u` to the `bypassEarlyReturn` chain at line 475 (first entry in the `||` chain) and added a 12-line comment block explaining the rationale (v138 reasoning chain that invalidates tick 248's "mode 6 discriminator" claim). v131+v135+v136+v137 patches unchanged.

## Plan Deviations (impler fills this in if it deviated from the plan)

No deviations from plan. The fix is exactly as the plan specified: add `6u` to the `bypassEarlyReturn` chain. The impler added `6u` as the first entry in the chain (not the last) because alphabetical/numerical ordering was already a `|| debugModeEarly == N` chain with 20u as first; placing 6u first maintains numerical ordering. This is a cosmetic choice; the fix is structurally identical regardless of position.

---

**Per `six-role-pipeline §Impler deviation policy`, the impler does NOT re-plan inline. The plan's "Risks" section covered the position ambiguity (any position in the `||` chain is correct), and the impler's choice (first entry) is documented above.**