# Pipeline Awaiting Parent — 2026-07-28

## State

The six-role pipeline is **actively waiting** for parent-supplied terminal evidence.

Since the v82 BLOCKER-HANDOFF pivot tick, the cron has reached its current tick (v83) and observed:

- `terminal` access remains structurally blocked. 4 distinct commands (`true`, `echo`, `date`, `pwd`) rejected this turn with `pending_approval: tirith:unknown`. The cron's `enabled_toolsets: ["terminal", "file"]` is declared but tirith continues to gate it.
- The newest dumps directory stamp remains `20260727_000706`-`000708` (36+ hours old). No parent has rerun the test since 2026-07-27 00:07.
- No `PIPELINE_GOAL_DONE_*.md` has been written.
- No `PIPELINE_PAUSED_*.md` has been written.
- The cumulative 22-patch inventory is intact (v41 alpha-encoder at FImageDump.cpp:27 re-verified this tick as the fresh probe).
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh` (v43) remains the parent-actionable read-only helper, unchanged from v32/v37/v43.

## Why this tick is not silent

Per the cron's prompt this turn, "do not silently stop" — so v83 lands as an **evidence-confirmation tick** rather than a silent exit. It writes this AWAITING_PARENT marker + 6 PENDING_*_v83.md markers + a PENDLE_HEALTH append + a PENDING_PICK update, *without* modifying any source file, *without* fabricating progress, and *without* cloning the v25-v81 standby pattern.

The cron is functional, not stalled. It's waiting for the parent's `fresh-evidence-scan.sh` + rebuild + rerun + validator output combination.

## Parent action required

The parent must supply the four pieces of evidence per `docs/PIPELINE_BLOCKER_2026-07-28.md` § "Minimum parent actions":

```bash
# Step 1 — confirm patches present (1 sec, no rebuild)
bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh

# Step 2 — rebuild (1-2 min)
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild

# Step 3 — rerun with frame dumps (10 sec)
cd Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
  ./TestReSTIR_GI_Temporal 2>TestReSTIR_GI_Temporal_stderr.log

# Step 4 — validate fresh dumps (1 sec)
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
```

Optional but recommended: open the freshly-dumped `display_frame8.png` in `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/` and confirm it contains recognizable non-uniform Sponza geometry.

## Deadline for v84

The cron's "do not silently stop" requirement applies forward from this tick. v83 is the deadline-bounded transition. v84 — the next tick — is the cron-must-decide point:

- If parent reply arrives between now and v84 → v84 routes to one of three branches (PASS → goal done; FAIL → FIX cycle; gi_raw=0 persist → diagnostic-handoff).
- If no parent reply arrives by v84 → v84 writes `docs/PIPELINE_PAUSED_2026-07-28.md` and the cron pipeline self-pauses. The next interactive session resumes the work.

The v82 PARTIAL_KEEP was correct: continuing the v25-v81 standby pattern for v83-vN without parent evidence would not produce diagnostic value. v83 is the structured pivot; v84 is the deadline-pause-or-resume gate.

## Why no source-code changes

The cumulative 22-patch inventory is intact. The bug symptom (`gi_raw R[0,0,0] G[0,0,0] B[0,0,0]` per stale 2026-07-27 log) is unchanged from the v25-v81 cycle start. No new evidence is available that would let a source patch be applied responsibly. Writing speculative fixes against a fixed memory of the bug — instead of fresh build/run/validate evidence — risks triggering the gpu-rendering-bisect-debug skill's "rebuild from ash" anti-pattern.

The right next move is the parent's terminal verification, not another series of file-only probes.

## Linked files

- `docs/PIPELINE_BLOCKER_2026-07-28.md` — the 4-command recipe; this AWAITING_PARENT is its "live" counterpart.
- `docs/PENDING_PLAN_v83.md` — v83 plan.
- `docs/PIPELINE_HEALTH_2026-07-28.md` — running health audit; this tick's append at the bottom.
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh` — read-only triage script; v43 (unchanged since v32).
