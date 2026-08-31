# Pending Plan v31

- task: v31 — structural standby tick (parent-evidence-gated continuation); terminal-block persists (tirith denies every probe: `pending_approval: tirith:unknown`); 0 source-code lines modified
- source: no bundle — direct disk audit
- approach: (1) Append v31 standby tick to `docs/PIPELINE_HEALTH_2026-07-27.md` recording (a) the latest terminal-probe results (multiple failed attempts this tick), (b) the 17-cycle patch inventory unchanged from v25/v26/v27/v28/v29/v30, (c) the persistent structural host block, (d) the canonical parent-triage recipe. (2) Update `docs/PENDING_PICK.md` to mark v31 [x] (this standby cycle is complete); re-stage v32 as parent-evidence-gated continuation. (3) Write 6 markers following discipline.
- diff_estimate: +0 / -0 source-code lines (documentation-only)
- skip_plan_review: no — documentation-only changes still follow marker discipline
- test_strategy: cron file-only (Part A static audit). Part B runtime gate is parent-driven.
- risks: none — pure documentation. The risk of repeated standby-tick cycles is mitigated by the explicit parent-evidence gate in v32 re-staging so the next parent-action is unambiguous.

## Why this cycle is documentation-only

Per the v29 audit's verdict (`docs/PENDING_TEST_AUDIT_v29.md` and `docs/PIPELINE_HEALTH_2026-07-27.md`):

> "v29 is a structural standby tick that records the exhausted file-only work space, the 17 patches on disk, and the canonical parent-triage recipe. No fabricated progress markers will be written on subsequent ticks while the structural terminal block persists."

Per the v30 audit's verdict (`docs/PENDING_TEST_AUDIT_v30.md`):

> "v30 cycle is complete. v31 is parent-evidence-gated per PICK's re-staging. The pipeline's file-only work space remains exhausted: v22 binding-layout-split is the load-bearing corrective candidate already on disk; v28 alpha-channel sentinel is the last meaningful file-only diagnostic-surface expansion already on disk; everything past v30 requires parent-driven terminal access (rebuild + run + dump + validate + vision). If the structural terminal block persists indefinitely, the pipeline stays at this heartbeat. The cron's 'Never silently exit' hard rule is satisfied by this tick; subsequent ticks can be identical standby cycles recording the same structural state, parent-action-required, until either the terminal block lifts or parent provides runtime evidence."

Confirmed this tick:

- Multiple terminal probes attempted (`echo probe-1 && date && pwd`), all blocked by tirith (`pending_approval: tirith:unknown`).
- `v22 binding-layout-split` is load-bearing; `v28 alpha-channel alive-sentinel` is the last meaningful file-only diagnostic expansion; both verified in source via read_file at documented line numbers (last tick was v30).
- Cron's prompt claims `enabled_toolsets: ["terminal", "file"]`, but the host denies every terminal probe. The instruction "this cron has terminal access" is contradicted by host policy.
- Per `gpu-rendering-bisect-debug` "Don't fabricate findings": if a step is blocked, report the blocker. The blocker is structural (host policy), persistent, and will only lift via (a) host policy update granting cron subagent shell, (b) parent manually performing the rebuild/run/verify steps and pasting evidence back, or (c) running the pipeline on a host with terminal-enabled subagent access.

## What this plan does NOT do

- Does NOT introduce a corrective fix (would require terminal to verify).
- Does NOT introduce another diagnostic sentinel (would be a duplicate of v28's alpha-floor probe).
- Does NOT commit, push, archive, pause, create Kanban cards, or modify governance.
- Does NOT fabricate parent evidence.

## Plan Deviations (impler fills this in if it deviated)

None. +0/-0 source-code lines.