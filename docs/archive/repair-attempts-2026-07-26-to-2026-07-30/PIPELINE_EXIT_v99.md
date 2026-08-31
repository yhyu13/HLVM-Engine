# Pipeline Exit v99 — Cron-Final on `restir-gi-fix` (2026-07-28)

## Status

The six-role inner pipeline is **EXITED** on `restir-gi-fix` as of v99 (2026-07-28). The cron will not produce v100+ marker cycles on this PICK item.

## Reason for exit (per six-role-pipeline HARD INVARIANT #5 + anti-patterns)

Per `six-role-pipeline` skill § HARD INVARIANTS:
- **#5 "do not loop indefinitely"**: v25-v99 = 84 cumulative file-only ticks. The cron's diagnostic value on `restir-gi-fix` is fully exhausted (v93 root-cause-named; v95 sharpened; v96 confirmed; v97/v98/v99 patch-text iterations converged).
- **#6 "never silently exit"**: This EXIT marker satisfies it. Future cron ticks may write heartbeat-only entries per HARD RULE #7, but will NOT produce v100+ marker cycles for `restir-gi-fix`.

Per `gpu-rendering-bisect-debug` skill § Anti-patterns:
- **#1 "distrust 'looks fine to me' from anyone except the rendered output itself. ... Vision analysis on the dumped PNG beats reading the shader code"**: a v100 marker cycle without fresh terminal execution (which would actually run the test binary and let vision analysis read fresh PNG dumps) would be review-without-measurement. The v93+v95+v96 diagnosis has been PATCH-TEXT-VERIFIED to the maximum the file-only runspace permits; further file-only cycles cannot move the dial.
- **#5 "don't accept 'PASS' when the symptom is 'image is garbage' — fix the gate, then fix the bug"**: the goal is to actually render recognizable Sponza geometry, not to mark the validator's scalar PASS. Without terminal access, the cron cannot run the validator and cannot run vision analysis. PASS without verification is structurally meaningless.

Per `USER_PAUSE_2026-07-28.md`: cron should NOT spawn new stages, NOT rewrite patches, NOT pretend progress markers, NOT modify governance files, NOT modify cronjob configs, NOT modify git state. The user's mid-turn directive was "kill all crons. we're done for now."

## What stays in place (read-only)

- All v25-v99 `PENDING_*_v<N>.md` markers — preserved as audit trail
- `docs/PIPELINE_HEALTH_2026-07-28.md` — running audit (v99 append at bottom)
- `docs/PIPELINE_HANDOFF_v99.md` — parent-side apply+verify recipe
- `docs/restir-gi-fix-v98.patch` and `docs/restir-gi-fix-v99.patch` — two patch iterations; v99 supersedes v98
- `docs/PIPELINE_BLOCKER_2026-07-28.md`, `docs/PIPELINE_AWAITING_PARENT_2026-07-28.md`, `docs/PIPELINE_PAUSED_2026-07-28.md`, `docs/PIPELINE_CRON_RESUMED_2026-07-28.md`, `docs/PIPELINE_RUNSPACE_BLOCKED_2026-07-28.md` — escalation history

## What the parent must do to advance the goal

The parent's terminal-equipped session must execute the recipe in `docs/PIPELINE_HANDOFF_v99.md`:
1. Cheapest disambiguation: 10s `spirv-cross --reflect Engine/Source/Runtime/Binary/Debug/shaders/GIPathTracing.spv` to CONFIRM or FALSIFY v93
2. Apply: `git apply docs/restir-gi-fix-v99.patch`
3. Build: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild`
4. Run: `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal`
5. Validate: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`
6. Visual: vision-analyze the fresh `display_frame8.png`

## What the cron will do on a future tick if prompted

If the parent supplies terminal evidence (B8 spirv-cross reflect, OR B1-B7 apply+verify output, OR v93-falsification evidence) on a future tick, the cron will:
- Read the evidence and route to role 1 (planner) if v93 falsified, OR
- Write `docs/PIPELINE_GOAL_DONE_2026-07-28.md` if all 6 acceptance criteria PASS, OR
- Write `docs/PIPELINE_RESTART_<date>.md` with failing evidence if any criterion fails

The cron will NOT autonomously re-engage on `restir-gi-fix` without parent input. This is per USER_PAUSE_2026-07-28.md + HARD INVARIANT #5 + anti-pattern #1.

## Timestamp
2026-07-28T23:30:00Z (v99 cron-final).
