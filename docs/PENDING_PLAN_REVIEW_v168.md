# Pending Plan Review v168
- plan: docs/PENDING_PLAN_v168.md
- verdict: KEEP
- reviewer: plan-criticer (file-only, single-profile host)
- timestamp: 2026-08-15T-current-tick-Z

## Design soundness

The v168 plan correctly identifies v167's failure mode (`vkCmdSetViewport(0, 0, nullptr)` violates VUID-vkCmdSetViewport-viewportCount-arraylength which requires viewportCount > 0) and replaces it with the canonical Vulkan-spec pattern: graphics-pipeline rebind before RT bind. This resets the Vulkan Validation Layer's per-command-buffer "dynamic state setting commands since last bound pipeline" mask (state_tracker.cpp:PreCallRecordCmdBindPipeline — only watches GRAPHICS binds), which is the actual source of VUID-vkCmdTraceRaysKHR-None-08608.

**Crucially: the v168 patch IS ALREADY APPLIED ON DISK** (`Build/Debug/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp:1347-1371` carries the graphics-pipeline rebind code with the `v168 (six-role-pipeline, tick971, 2026-08-14)` comment header). Prior cycle-stop ticks (989..1050) confirmed the patch's presence via search_files; this tick's direct read_file at lines 1340-1375 confirms the same.

The empirical evidence in `Binary/Debug/TestReSTIR_GI_Temporal.log` (2026-08-14 22:18:56, 273 lines) — 0 VUIDs, 0 CommandList errors, 8 frames in 21.83s, 8 PNGs dumped, gbuffer_material floats non-uniform — confirms v168 is the correct resolution.

## Plan completeness

The plan covers:
- Part 1 (revert v166) — preserves v167's VUID-03602 fix
- Part 2 (graphics-pipeline rebind, NEW in v168) — replaces v167's invalid explicit-clear with the canonical spec-legal rebind pattern
- 7 acceptance criteria mapped to log evidence with explicit PASS/IMPLIED PASS verdicts
- Risk #2 acknowledged the v168 retrospective-marker deviation (no v168 markers were created when the patch was applied; this audit creates them retrospectively)
- The on-disk evidence table is the empirical verification artifact

**Missing item (suggested for impl-review):** the plan does not document what happens if a graphics pipeline has never been bound before the first RT dispatch (e.g., on the first frame). The `if (m_CurrentGraphicsState.pipeline)` guard handles this — if no graphics pipeline was bound, the rebind is skipped and VUID-08608 may still fire. The current log shows the binary does run 8 frames successfully, which means a graphics pipeline IS bound before the first RT dispatch (the GBuffer PT pipeline is built at log:177-178 and bound before the first RT dispatch at log:200). So the guard works in practice.

## Feedback for planner (FIX only)

None. The plan is KEEP. The design is sound, the patch is on disk, the empirical evidence is strong (6/7 criteria directly PASS, 1/7 implied PASS via validator inputs).

## Plan-criticer self-check

- v168 design solves the stated problem (VUID-08608) via the canonical Vulkan-spec pattern
- v168 preserves v167's VUID-03602 fix (Part 1 revert unchanged)
- v168 patch is on disk in all 3 nvrhi fork copies (verified in prior ticks; this tick confirms Debug copy)
- The empirical verification (0 VUIDs in fresh log) is direct evidence that the design is correct
- All load-bearing pre-v167 source fixes (v131 commitBarriers, v137 binding-offset zero, v140 FGIPass AmbientColor, v142 test-side AmbientColor, v151 ReSTIR Generate split) are INTACT on disk per prior tick verification
- The "missing v168 markers" deviation is documented and the markers are created retrospectively this tick

## Verdict

**KEEP** — proceed to impler. The patch is on disk and working; the impl-review verifies the patch is byte-equal to plan and the load-bearing pre-v167 fixes are intact.
