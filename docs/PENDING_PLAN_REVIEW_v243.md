# Pending Plan Review v243

- **plan**: docs/PENDING_PLAN_v243.md
- **verdict**: KEEP
- **reviewer**: plan-criticer (this tick)
- **timestamp**: 2026-12-15 (cron invocation #1163)

## Design soundness

The plan correctly identifies that v140 (AmbientColor override) + v182 (probe-position fix) have already been applied to source, and that the only remaining work is empirical verification of those fixes. The three-phase (build → analyze → accept) shape matches `software-development-practices §Path-Tracing / RT Debugging Methodology`. The five debug-mode runs (20/21/22/30/31) cover the discriminator matrix: SRV readability (20/21/22), sentinel at corner (30), and slangc dead-strip (31). The acceptance contract maps cleanly to the 7 gates in the user instruction.

## Plan completeness

**Strengths:**
- Acknowledges HARD INVARIANT #1 (PICK authoritative) and HARD INVARIANT #6 (always write SOMETHING to PIPELINE_HEALTH)
- Maps each acceptance criterion to the file-only or terminal-required evidence type
- Surfaces the 5 risks including the central tirith blocker
- Identifies that mode 31 is the slangc-dead-strip discriminator per `gpu-rendering-bisect-debug §The decisive experiment: constant-sentinel reads across the boundary`

**Gap (FIX candidate, downgraded to "minor"):**
- The plan does not specify what to do if mode 30 shows magenta but mode 20 shows zero everywhere — this is the "partially bound" scenario described in risk #3. Add a contingency: if mode 30=magenta AND mode 20=zero, the next cycle (v244) should investigate the layout-transition ping-pong between UAV and SRV bindings.
- Mitigation: this is implicit in the `## Impler deviation policy` of the six-role skill (impler fills in `## Plan Deviations` in PENDING_COMMIT); the impler can note it there.

## Feedback for planner (FIX only)

N/A — verdict is KEEP. The two minor improvements above can be addressed by the impler as `## Plan Deviations` if they materialize during the empirical run.

## Cross-check against authoritative current-state docs

- `docs/DIAGNOSTIC_2026-07-30.md` (155L): the original v24 hypothesis (binding broken → mode 20 returns zero). Plan correctly treats this as superseded by v182 source fix.
- `docs/DIAGNOSTIC_2026-08-01-v25.md` (211L): the v25 hypothesis shifted to AmbientColor hardcoded. Plan correctly identifies v140 source fix addresses this.
- `docs/DIAGNOSTIC_2026-08-30-state-machine-617.md` (157L): canonical state machine. Plan correctly references the v182 fix at `GIPathTracing.hlsl:499-503/524/584/757-766/793`.

## Verdict

**KEEP** — plan is sound, complete, and actionable. The single substantive concern (terminal denial) is reported in the plan's risks section as a blocker, not a defect in the plan itself.