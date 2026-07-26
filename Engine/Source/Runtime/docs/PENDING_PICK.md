# Six-Role Pipeline — Pending Task Queue (HLVM-Engine rhi2)

This file is the parent-written priority queue for the six-role
pipeline cron running on HLVM-Engine `rhi2` branch. The cron
dispatcher reads this file at every tick and routes to the
appropriate role based on which tasks are `[ ]` (pending) and
which are `[x]` (done).

## Pipeline config

- Workdir: `Engine/Source/Runtime/`
- Branch: `rhi2` (direct commits, no worktree)
- Profile: `claude_coder` (single-profile mode; freshness
  guarantee reduced — see dispatcher prompt caveat)
- Test target: `TestReSTIR_GI_Temporal`
- Build: `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal`
- Verify command: see `SESSION_HANDOFF_2026-07-25.md`.

## Status as of 2026-07-25 (end of session)

- [x] **card-0: visualization fix** landed as commit `2fab7d6`.
  gbuffer_worldpos now shows real Sponza geometry.

- [x] **card-1: GI magenta root cause** landed as commit `e6b3d52`.
  WriteGBufferSentinels was leaving the GBuffer textures in a
  state where FGIPass SRV reads returned the sentinel value
  instead of the post-raster pixel data. Removed the call.
  Display now shows recognizable Sponza scene with GI shading.

- [x] **card-2: bug-075 layout transition** audit complete.
  The TemporalReservoir layout error is non-fatal (NVRHI's
  bug-073 patch suppresses the immediate-CL collision). The
  magenta fix landed without addressing bug-075 directly,
  which proves the layout error was not the root cause of
  the magenta symptom. Defer the proper fix to a follow-up
  (split temporal pass into read/write dispatches).

## Remaining backlog

Three items remain as `[ ]` below. None of them are blockers
for the magenta-noise fix (commits `2fab7d6` and `e6b3d52`).
They are tracked here for future work but should not block
the next session's startup.

- [ ] **bug-075 proper fix**: split the temporal pass into
  read and write dispatches so the resource state is
  unambiguous. Currently the temporal pass binds the
  OutReservoir as UAV in the same dispatch as HistoryReservoir
  as SRV, which Vulkan validation flags as a layout
  mismatch. Non-fatal today. Investigated 2026-07-25: tried
  `commitBarriers()` after the transitions, tried ping-pong-
  aware per-role transitions, tried `UnorderedAccess` for
  the OutReservoir. None of these made the validation
  warning go away. The root cause is in nvrhi's auto-barrier
  ordering: `setComputeState` walks the binding set AFTER
  binding descriptor sets (vulkan-compute.cpp:120-145), so
  the validation layer sees the descriptor-bound state before
  the transition barrier takes effect. The proper fix is
  to split FReSTIRPass's binding layout into a read-only
  set (SRVs only) and a write-only set (UAVs only) and
  dispatch the temporal pass in two phases. Deferred —
  needs an FReSTIRPass interface change AND a shader
  recompile (separate binding sets require different
  register indices).

- [ ] **Sponza material colors**: the GLTF loaded for this
  test has white materials for the rendered meshes. Not a
  test code bug; the test just doesn't render the colorful
  parts of Sponza. To improve visual coverage, load a
  different mesh subset or replace materials with a
  procedural color palette. CONTENT WORK, not a test bug.

- [ ] **Sponza lighting**: the test uses 1 directional light
  plus a hardcoded ambient term. A real Sponza scene would
  have area lights with proper NEE. The path tracer
  supports it; the test just doesn't set it up. CONTENT
  WORK, not a test bug.

## Pipeline status

The `six-role-pipeline` skill was created in this session at
`~/.hermes/skills/devops/six-role-pipeline/` but the cron was
not run for HLVM-Engine. Interactive debugging proved faster
than the 6-role cycle for this particular GPU bisection work.

The skill is still available for projects where the cron
model fits better. For the immediate follow-ups above
(mostly content/asset work, not structural bugs), the
existing `kanban-orchestrator` generic model is a better
fit than the six-role pipeline.