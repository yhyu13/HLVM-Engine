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

## Remaining backlog (lower priority, not blockers)

- [ ] **bug-075 proper fix**: split the temporal pass into
  read and write dispatches so the resource state is
  unambiguous. Currently the temporal pass binds the
  OutReservoir as UAV in the same dispatch as HistoryReservoir
  as SRV, which Vulkan validation flags as a layout
  mismatch. Non-fatal today; should be cleaned up for
  production.

- [ ] **Sponza material colors**: the GLTF loaded for this
  test has white materials for the rendered meshes. Not a
  test code bug; the test just doesn't render the colorful
  parts of Sponza. To improve visual coverage, load a
  different mesh subset or replace materials with a
  procedural color palette.

- [ ] **Sponza lighting**: the test uses 1 directional light
  plus a hardcoded ambient term. A real Sponza scene would
  have area lights with proper NEE. The path tracer
  supports it; the test just doesn't set it up.

- [ ] **Dead code removal**: `WriteGBufferSentinels` and
  `FillGBufferHardcoded` in `TestReSTIR_GI_Temporal.cpp`
  are unused after commit `e6b3d52`. Safe to delete but
  kept for reference.

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