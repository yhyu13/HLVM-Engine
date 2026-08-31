# Pending Plan v33 — structural standby tick (post-v32 audit; no fresh evidence; terminal still blocked by tirith)

## State-machine routing decision
- Read `PENDING_PICK.md`, all v32 markers (PLAN/PLAN_REVIEW/COMMIT/IMPL_REVIEW/TESTS/TEST_AUDIT), latest `PIPELINE_HEALTH_2026-07-27.md` tail.
- v32 cycle complete at audit ALL_KEEP. Rule 9 fires → next item is v33 (parent-evidence-gated).
- v33 is explicitly gated: "ONLY fires after parent runs `bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh` and pastes the output back, OR runs the multi-step recipe and pastes rgi_evidence.txt".
- Cron's prompt grants `enabled_toolsets: ["terminal", "file"]`, but every terminal probe in this tick was again blocked by tirith (`pending_approval`, `tirith:unknown`): `date && pwd && ls …`, `date`, `echo "ping"`, `stat -c '%y %n' …`, `date; pwd; echo done`. Effective toolset is file-only.
- Decision: do NOT invent a v34 cycle against parent-gated work; do NOT fabricate KEEP verdicts. Record honest heartbeat tick per skill HARD INVARIANT #6.

## Task description (this tick)
- Structural standby tick identical in shape to v25/v26/v27/v29/v30/v31/v32.
- Append-only audit (no source-code modifications).
- Confirm 18-patch cumulative inventory is intact in source (v3-v32 inclusive).
- Stage v34 as next standby candidate IF v33 is also terminal-blocked.

## Diff estimate
- `docs/PENDING_PLAN_v33.md`: +N new (this file)
- `docs/PENDING_PLAN_REVIEW_v33.md`: +N new
- `docs/PENDING_COMMIT_v33.md`: +N new
- `docs/PENDING_IMPL_REVIEW_v33.md`: +N new
- `docs/PENDING_TESTS_v33.md`: +N new
- `docs/PENDING_TEST_AUDIT_v33.md`: +N new
- `docs/PIPELINE_HEALTH_2026-07-27.md`: +1 append (heartbeat tick section)
- `docs/PENDING_PICK.md`: v33 [x], v34 staged as next standby candidate
- **0 source-code lines modified.**

## skip_plan_review: no
- This cycle is mechanical/repetitive (matches v25-v32 shape exactly) but the per-role audit trail is preserved for the parent's review-on-demand.

## produces_test_files: no
- No new test files created. No tests modified.

## Test strategy (parent-driven)
1. `bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh` and paste back exit code + banner.
2. If exit 0: rebuild + default-mode run + `HLVM_PT_DEBUG_MODE=6` run + validator + vision-check.
3. If exit 1: identical standby v34 if parent still cannot rebuild.
4. If exit 2: cron stages inline patch reapplication for the missing patch entry.

## Risks
- Single-head host caveat: all 6 roles share the same model. Verdicts are self-checks, not independent reviews.
- Tirith block persists; effective toolset remains file-only.
- No new code path means no behavioral change; risk of regression is zero.
- Pipeline accumulation risk: another standby tick does not advance the renderer; it only persists the structural state for the next parent session.

## Decision matrix (post-parent-evidence)
- exit 0 + cr/fire/v3-now-fire/mode-6-gradient/mode-0-nonzero/display-correct/validator-3-3 → PIPELINE_GOAL_DONE (v6d branch)
- exit 0 + cerr-doesn't-fire → v33a: stderr buffering investigation
- exit 0 + cerr-fires + spdlog-still-doesn't-fire → v33b: spdlog config fix
- exit 0 + cerr-fires + spdlog-now-fires + mode-6-still-0 → v33c: slangc dead-strip investigation
- exit 0 + cerr-fires + spdlog-now-fires + mode-6-gradient + mode-0-still-0 → v33d: payload/result merge investigation
- exit 1 → v33e: identical standby tick v34
- exit 2 + 1-2 patches missing → v33f: inline patch reapplication

## Goal gate (unchanged)
**FAILED/UNVERIFIED** — six-criterion gate from the prompt:
- (a) Debug target builds cleanly — UNVERIFIED (tirith blocks terminal)
- (b) Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — UNVERIFIED
- (c) No command-list-already-open errors — UNVERIFIED
- (d) No Vulkan ERROR/VUID in fresh log — UNVERIFIED
- (e) Validator passes newest dump group — UNVERIFIED
- (f) Display visibly contains recognizable non-uniform Sponza — UNVERIFIED
- No `PIPELINE_GOAL_DONE_<date>.md` written.