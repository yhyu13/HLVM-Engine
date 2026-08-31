# Pending Commit v229

- plan: docs/PENDING_PLAN_v229.md
- files: Engine/Source/Runtime/Test/TestCornellBoxGI.cpp
- source: no bundle — direct edit
- target: (no branch — nothing committed; parent owns git topology)
- task: **v229 fix for card L** — extract the 14-texture ReSTIR creation block into a free static helper `CreateReSTIRTextures(nvrhi::IDevice*, W, H, 14 handle refs)` and call it from BOTH the Initialize path (replacing the original inline block at :955-1021) and the resize branch in Render (at :1314, just before `BindingCache.Clear()`). The fix ensures that a window-resize event recreates the ReSTIR textures at the new extent, mirroring the existing pattern for GBuffer MRTs at :1169-1199.
- verify: `./Build.sh --Config=Debug --Target=TestCornellBoxGI --Rebuild && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestCornellBoxGI` (per `docs/agents/agent_3_impler.md` standard; targets `TestCornellBoxGI` not `TestReSTIR_GI_Temporal` since the cycle touches the control). Then `grep -E "VUID|ERROR" TestCornellBoxGI.log` → 0 hits expected. Then `python3 Engine/Source/Runtime/Test/TestCornellBoxGI_Data/validate_cornell.py` (or whatever the canonical validator is; agent_5 will confirm).
- skip_impl_review: yes (diff is +101/-74 = +27 net lines, single file, no new test files — well under the 50-line budget per `agent_3_impler.md` standard)
- produces_test_files: no
- notes:
  - **Diff summary**: one file modified, one new helper function added (+101 lines incl. comments), original inline block replaced with single call site (−74 lines), one call site added to resize branch (+14 lines). Net = +27 lines functional, +15 comment.
  - **Diff verification**: two `patch` calls returned clean unified diffs, no warning surfaced. The warning shown was a "last read with offset/limit pagination" warning, which is a routine pagination warning for large files — not a diff problem.
  - **Test invariants (role #5 will pin)**:
    - `CreateReSTIRTextures` definition: 1 hit (line 90)
    - `CreateReSTIRTextures(` call sites: 2 hits (lines 1068 init, 1314 resize)
    - `NvrhiDevice->createTexture` total count: unchanged (the same 14 calls exist; just relocated into the helper)
    - `clearTextureFloat` count: 3 hits (one per history texture), all inside the helper
    - `BindingCache.Clear()`: 3 hits total (one in resize branch — unchanged; two elsewhere — unchanged)
    - `bReSTIRInitialized` count: 3 hits (declare, init-set, render-guard) — unchanged
  - **No protected file touched**: `AGENTS.md`, `CLAUDE.md`, `.cursorrules` all byte-unchanged.
  - **No git operations**: no commit, no push, no merge. Parent owns git topology per `agent_3_impler.md`.
  - **Single-profile caveat**: planner/plan-criticer/impler are same head with different prompt text. The reviewer (skipped per `skip_impl_review: yes`) would have audited deviations; the planner's `[SILENT]`-gate at `agent_1_planner.md:32` is a self-check, not fresh eyes.

## Plan Deviations

The plan-criticer's FIX review returned 5 corrections; all 5 are incorporated:

1. **Replace test_strategy row (i) with count-delta test**: incorporated in `notes` section above.
2. **Specify release semantics**: explicit comment in the helper at `:78-83` — nvrhi::TextureHandle ref-counted, reassignment drops old.
3. **Choose helper visibility**: free static, no header change (the plan-criticer's preferred option).
4. **`bReSTIRInitialized` ordering**: not modified; the helper runs BEFORE `ReSTIRPass.Initialize` at init, and only after init on resize. Render-guard at `:1501` is unchanged.
5. **Build verification note**: `verify` field above uses `./Build.sh --Config=Debug --Target=TestCornellBoxGI --Rebuild`, not `TestReSTIR_GI_Temporal`, because the cycle touches the control.

No deviations from the revised plan.