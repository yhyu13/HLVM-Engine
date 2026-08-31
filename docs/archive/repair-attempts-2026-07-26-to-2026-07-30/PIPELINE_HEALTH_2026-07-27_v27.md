# PIPELINE_HEALTH_2026-07-27 — v27 inner-pipeline heartbeat (post-v26; no new cycle fired)

## State-machine routing decision

- Read `PENDING_PICK.md`, the most recent cycle markers (v24 ALL_KEEP; v17/v15/v13 sync patches on disk; v16 corrected-understanding doc), the v25/v26 inner-pipeline heartbeats, and the v26 outer-watchdog post-v26 file.
- Re-probed terminal in this tick (1 probe: `pwd && date`) — blocked by tirith with `status=pending_approval, tirith:unknown`. Pattern matches 15+ prior ticks. Effective toolset is file-only despite the cron's terminal grant.
- The cron's "continue cycles ... do not silently stop" instruction authorizes mechanically-actionable file-only fixes. v24 already added the canonical file-only advance available: `dump_pixelstats.py` (5-second fast first-look on stale dumps). v16 corrected which HLSL Private master slangc compiles. v17 added TraceRay-bypass sentinel (case 7u) to that same Private master. The diagnostic surface is now COMPLETE for the parent-driven evidence shape, but the renderer itself remains broken pending terminal-evidence.
- Per `six-role-pipeline` HARD INVARIANT #6 ("Never silently exit") and the cron's "If blocked by an external issue, record exact evidence in a marker" instruction: this v27 heartbeat records the structural block honestly and exits.

## Static disk-evidence audit (this tick)

- **`Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl`**: now 792 lines, 31766 bytes. `case 6u` at line 593 (v13/v15 patch, load-bearing per v16). `case 7u` at line 604 (v17 TraceRay-bypass sentinel, byte-identical in Private master and data-dir copy). File scope verified by read_file at offset 575-604 and search_files matching 3 distinct CMakeLists references (lines 389, 1554, 1877) all pointing at this Private master path.
- **`Engine/Source/Runtime/ShaderMakeBuild.py:613`**: `gi_shader_dir + "/GIPathTracing.hlsl"` is what the `create_restir_gi_temporal_shadermake` factory passes to ShaderMake → slangc. `gi_shader_dir` resolves to `${CMAKE_SOURCE_DIR}/Private/Renderer/Shader/GI` (verified at ShaderMakeBuild.py:571 and surrounding context). This confirms the v16 corrected understanding: Private master is what slangc compiles.
- **PENDING_PICK.md queue**: v1–v17 all marked `[x]` (or explicitly status-flagged); remaining unchecked is v22 (binding-layout-split fix) gated on parent's `rgi_evidence.txt`. No other `[ ]` items exist; the queue is at the parent-gate step.
- **No fresh build artifacts**: stale `dumps/20260727_000706`–`000708` group remains the latest evidence. No `stderr.log`. No `rgi_evidence.txt`. No fresh `display_frame*.png`. No `PIPELINE_NUDGE` or `PIPELINE_GOAL_DONE` markers present (search_files confirmed 0 matches for both).
- **v24 markers remain the latest completed cycle** (PLAN_REVIEW=KEEP, IMPL_REVIEW=KEEP, TEST_AUDIT=ALL_KEEP). v17 has all six markers but v17 is a diagnostic surface add (not a renderer fix). v22 (impl cycle) is gated on parent evidence.

## Why no new cycle fired this tick

The cron's "continue cycling" instruction authorizes mechanically-actionable file-only fixes. Available file-only actions remaining:

| Candidate | Status | Why rejected |
|-----------|--------|--------------|
| Fire v18/v19 (add more debug mode sentinels cases 8/9/10/11/12/15 to GIPathTracing.hlsl) | Already executed in prior cycle | The diagnostic surface is already complete (cases 1-15 + TraceRay-bypass + default-case trace). Adding more sentinels without evidence is scope creep. |
| Run `dump_pixelstats.py` against stale dumps | Terminal-blocked; `python3` is a terminal command | The tool file is staged and 13/13 Part A static tests pass; Part B runtime verification is parent-driven per v24 plan. |
| Patch the Private master to a corrective fix | Revoked without evidence | The cron's "do not silently stop" rule does not authorize code fixes against a parent-gated PICK item. v22 is gated on `rgi_evidence.txt` for binding-layout-split. |
| Fabricate v22 markers | Explicitly forbidden | Per HARD INVARIANT #6 + the cron's "never fabricate results" instruction + `software-development-practices` "Don't fabricate findings." |
| Write a v27 cycle that re-iterates the diagnostic surface | Scope creep | There is no additional file-only artifact beyond what v17 already provides. |
| Refresh the dispatcher's internal queue | No-op | `PENDING_PICK.md` is correct as it stands; v22 is the legitimate next gate. |
| Synthesize a v28 prepatch that anticipates parent evidence | Forbidden | Speculative work without evidence; violates "Never fabricate results." |

The honest position: v24 closed the file-only diagnostic surface (dump_pixelstats.py). v17 added the final TraceRay-bypass sentinel that completes the diagnostic ladder (modes 1-15 + TraceRay-bypass). The remaining work to advance the renderer toward correctness requires parent-driven terminal evidence (rebuild + run + per-mode dump analysis + validator + vision analysis), which tirith is structurally blocking on this host.

## Final-goal gate (unchanged)

**FAILED/UNVERIFIED — same as 15+ prior ticks.** Six-criterion gate:

- (a) Debug target builds cleanly — UNVERIFIED (terminal blocked; `build_Debug.log` predates v13/v15/v17 patches; binary is stale)
- (b) Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — UNVERIFIED (no fresh dump group, no `stderr.log`)
- (c) No command-list-already-open errors — UNVERIFIED (stale log shows 7 warnings per run; v22 staged to address via binding-layout split)
- (d) No Vulkan ERROR/VUID in fresh log — UNVERIFIED (stale log has 0, but staleness disqualifies as fresh evidence)
- (e) Validator passes newest dump group — UNVERIFIED
- (f) Display visibly contains recognizable non-uniform Sponza with sane exposure — UNVERIFIED (vision unavailable, terminal blocked)
- (g) Relevant checks pass — UNVERIFIED

No `PIPELINE_GOAL_DONE_<date>.md` written. No `PIPELINE_NUDGE_<date>.md` written (this is a documented structural block, not an unexplained stall).

## Stall assessment

- **Documented evidence failure, not a stall.** Per the skill: an intentional parent-evidence wait with explicit evidence of why each criterion is unverified is not a stall; it is a documented structural limitation. No `PIPELINE_NUDGE` warranted.
- The cron's terminal toolset remains blocked by tirith on every probe. This is consistent with 15+ prior ticks. The cron cannot manufacture a workaround for this without violating the "do not silently stop" and "never fabricate" rules.
- The 17 cycles that have landed on disk (v1-v17 = 15 source patches + 3 script-only files: `run_rgi_diagnostic.sh`, `dump_pixelstats.py`, `dump_pixelstats.cpp[=0B placeholder]`) collectively represent the maximum file-only investment possible without terminal access. The remaining work is irreducibly terminal-driven.

## Hard invariants verified this tick

- (1) `PENDING_PICK.md` authoritative — yes; v22 remains the next `[ ]`; no synthetic insertion.
- (2) Test-files trigger reviewer — N/A (no cycle fired).
- (3) Impler deviation documentation — N/A (no cycle fired).
- (4) Plan-criticer FIX loops to planner — N/A (no cycle fired).
- (5) Single-instance lock — N/A in file-only mode.
- (6) "Never silently exit" — this v27 heartbeat satisfies it.

## Action taken this tick

- Read `PENDING_PICK.md`, the v24 + v17 markers, the v25/v26 inner heartbeats, the v26 outer post-v26 file.
- Read GIPathTracing.hlsl at offset 575-604 to verify v13/v15/v17 sentinels are intact at the correct lines.
- Searched CMakeLists.txt and ShaderMakeBuild.py for `GIPathTracing.hlsl` references (3 + 6 matches, all Private master).
- Re-probed terminal 1 time (`pwd && date` — blocked by tirith).
- Searched for `PIPELINE_NUDGE` and `PIPELINE_GOAL_DONE` markers (0 matches each, confirming gate is still failed and no prior nudge/done was written).
- Wrote this v27 inner-pipeline heartbeat.
- Did NOT: fire a new v18/v22 cycle (no mechanically-actionable file-only fix beyond v17), apply any source-code changes against the parent-gated v22 item, create Kanban cards, commit, push, archive, pause, modify governance, drift into interactive debugging, fabricate KEEP/ALL_KEEP verdicts, or claim success without evidence.

## Parent action required (UNCHANGED, slightly consolidated)

The minimum-action unblock is the 2-step triage path documented in v24/v25:

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine

# Optional Step 1 (5 seconds): fast first-look on stale dumps (no rebuild required)
rm Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.cpp  # remove 0-byte placeholder
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py

# Step 2 (10 minutes): full per-mode evidence via FIXED diagnostic script
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
cd Engine/Source/Runtime/Binary/Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log
HLVM_PT_DEBUG_MODE=6 HLVM_DUMP_RGI=1 ./TestReSTIR_GI_Temporal 2>stderr_mode6.log
HLVM_PT_DEBUG_MODE=7 HLVM_DUMP_RGI=1 ./TestReSTIR_GI_Temporal 2>stderr_mode7.log
bash ../../Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh

# Step 3: paste evidence back to cron
cat stderr*.log Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log \
    Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/rgi_evidence.txt
```

The cron will route to v22 (v21a binding-layout-split fix) or v21b..v21i based on the evidence shape per the v13a decision matrix in `PENDING_PICK.md`. v17 mode 7 (TraceRay-bypass sentinel) is the canonical next-step probe if mode 6 produces expected output.

## Cron tick summary (≤8 lines for delivery)

- State: v17 cycle complete (TraceRay-bypass sentinel case 7u in Private master). v24 dump_pixelstats.py + run_rgi_diagnostic.sh staged at data dir. v22 still `[ ]` and gated on parent running the FIXED diagnostic.
- Static evidence: terminal blocked by tirith on this tick (1/1 probes fail with `pending_approval, tirith:unknown`); effective toolset file-only. v13/v15/v17 sentinels verified at the Private master (file=792 lines, 31766 bytes; case 6u @ line 593, case 7u @ line 604).
- Final-goal gate: FAILED/UNVERIFIED on all 6 criteria (no fresh build/run/log/dumps/validator/visual).
- Action: re-read PICK + v17 markers + v25/v26 heartbeats + GIPathTracing.hlsl offset 575-604 + CMakeLists + ShaderMakeBuild.py; re-probed terminal (blocked); searched nudge/done markers (none); wrote v27 heartbeat.
- Did NOT: fabricate KEEP verdicts, apply v22's binding-layout-split without evidence, create Kanban cards, commit, push, archive, pause, modify governance, or fire a no-op cycle.
- Parent action: 2-step triage path ready (dump_pixelstats.py 5s, then run_rgi_diagnostic.sh 10min). v17 mode-7 evidence runs with `HLVM_PT_DEBUG_MODE=7` after rebuild.
- Pipeline remains incomplete pending parent-runner-evidence. v17 is the last file-only diagnostic cycle; v22 is the impl cycle, gated on parent verification.
- This v27 heartbeat exits with HARD INVARIANT #6 satisfied ("never silently exit") and the structural block recorded honestly per the cron's "record exact evidence in a marker" instruction.
