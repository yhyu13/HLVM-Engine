# Pending Test Audit v94
- tests: docs/PENDING_TESTS_v94.md
- commit: docs/PENDING_COMMIT_v94.md
- verdict: RUNSPACE_BLOCKED (new semantic, distinct from PARTIAL_KEEP* / ALL_KEEP* / ROOT_CAUSE_NAMED)
- verifier: testing-verifier (single-profile, file-only runspace)
- timestamp: 2026-07-28T23:50Z

## Broken-pattern audit
- [x] No from-x-import-y patch propagation bugs — N/A (no patch produced)
- [x] No test-bug-in-itself — N/A (verification-only tick)
- [x] No source-incomplete-relative-to-test — diagnosed source-completeness gap (v22 split half-applied), cross-tick verified intact
- [x] No missing test isolation fixture — N/A
- [x] No AsyncMock on sync function — N/A (C++ GPU pipeline, no AsyncMock)

## Per-test verdict
- Part A spot-checks: 6/6 PASS — v93 ROOT_CAUSE_NAMED diagnosis is NOT stale; all 6 file-only findings intact on disk between v93 and v94
- Part B probes B1-B8: 8/8 UNVERIFIED — terminal blocked; cannot satisfy any of the 6 acceptance criteria from this runspace

## Audit summary
**The cron's role on `restir-gi-fix` has shifted from "advance the diagnostic chain" (v25-v93, 69 cumulative ticks) to "stop looping without terminal evidence" (v94).** This shift is forced by HARD INVARIANT #5 ("do not loop indefinitely") + the gpu-rendering-bisect-debug skill's anti-fabrication rule + the v87 RUNSPACE_BLOCKED posture (which the cron's prompt this turn re-invokes via the 6-criterion acceptance gate).

**Cumulative narrowing chain (v25 → v94)**:
- v25-v81 (57 ticks): structural standby — 22-patch inventory verified intact, no root cause
- v82-v84 (3 ticks): blocker-handoff / deadline-pause — escalated to parent
- v85 (1 tick): cron-resumed per parent instruction
- v86-v88 (3 ticks): more structural standbys, one new finding (gi_raw=0,0,0 may be different bug class)
- v89 (1 tick): binding-wiring-narrowing (3-way hypothesis → bug is downstream of binding setup)
- v90 (1 tick): dumper-handle-chain narrowing (2-way hypothesis → eliminated dumper-side)
- v91 (1 tick): slot-validity collapse (1-way hypothesis → all 3 binding sites converge on slot 0)
- v92 (1 tick): prompt-vs-runspace divergence noted
- v93 (1 tick): ROOT_CAUSE_NAMED — v22 split half-applied to FGIPass; ~10-line fix bounded; sibling-correct-shape evidence at FReSTIRPass + ReSTIR_Temporal_cs.hlsl
- v94 (1 tick, this): RUNSPACE_BLOCKED — re-confirms v93 intact on disk, pivots cron posture to parent-evidence-gated

The cron's value across v25-v94 is the diagnostic chain that converged on a structural root cause at v93. The cron's value going forward is **zero on this PICK item** until parent supplies terminal evidence.

## Cron posture change for v94+
- PICK: `restir-gi-fix` marked `[ ]` with parent-evidence-gated status
- HARD INVARIANT #6 ("never silently exit") satisfied by this marker set + HEALTH append + PICK update
- HARD INVARIANT #5 ("do not loop indefinitely") satisfied by NOT running another standby tick on `restir-gi-fix` without parent terminal evidence

## What parent needs to do (unchanged from `PIPELINE_RUNSPACE_BLOCKED_2026-07-28.md`)
- Option A (recommended): reconfigure the cron to grant terminal access (`cronjob action="update"` setting `enabled_toolsets: ["terminal", "file"]`); then re-engage.
- Option B: execute the 4-command recipe from any terminal-equipped session and paste output:
  ```
  bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh && \
  ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild && \
  HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>TestReSTIR_GI_Temporal_stderr.log && \
  python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
  ```
- Option C: pause the cron (`cronjob action="pause"` on the HLVM-Engine cron job); continue with interactive `software-development-practices §Path-Tracing / RT Debugging Methodology`. The v93 diagnosis is the head-start.