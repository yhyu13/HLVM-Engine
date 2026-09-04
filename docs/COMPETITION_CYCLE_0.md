# Competition Cycle 0 — Brief

**Started:** 2026-09-02
**Mode:** v2 file-only (cron proposes, parent executor runs shell)

## Active profile
`conserver-gi` (top of `COMPETITION_QUEUE.md`)

## Last score summary
None yet — this is cycle 0. The competition is bootstrapping. First
score establishes D0 baseline for all future delta computations.

## Target dimension (predicted)
**D2 — Light transport** (weight 2.0, biggest expected overnight lift).

The project just shipped the v236 sky-bounce fix (per
`docs/DIAGNOSTIC_2026-07-30.md` and the JUNE 2026 commit trail).
The most likely first-win dimension is D2 — getting the indirect
sky contribution to register on shadow-side surfaces in the
Cornell Box.

Secondary targets (if D2 is already at 8/10 from prior work):
- D3 noise (filter quality)
- D1 PBR (material response)
- D6 temporal (TAA stability)

## Expected deliverables

1. `docs/PENDING_BUILD_cycle_0_round_0.md` — proposed diff (≤200
   lines) targeting D2. Written by `conserver-gi` via cron.
2. `docs/BUILD_RESULT_<id>.md` — parent executor's output (build
   status, render status, dump sha256). Written by you (or a
   delegated subagent with terminal access).
3. `docs/SCORES/cycle_0_round_0.md` — 6-dimension score breakdown.
   Written by `scorer` via cron (file-only, reads dump + reference).

## Wall-clock budget
- Cron tick: every 30 min. First tick at +30 min from cron start.
- Conserver proposal: ≤ 10 min (one diff, ≤ 200 lines).
- Parent executor: ≤ 10 min (build ~2 min, render ~1 min).
- Scorer: ≤ 5 min (read dump + ref, compute, write).
- Total per cycle: ~25 min. Cron tick at 30 min gives buffer.

## Pre-conditions (parent must satisfy BEFORE first tick succeeds)

1. ✅ `docs/COMPETITION_QUEUE.md` staged (done 2026-09-02).
2. ⏳ `docs/reference_renders/cornell_box_reference.exr` —
   parent must bake Cycles ground truth (4096 SPP, linear EXR)
   and commit.
3. ⏳ Cron `hlvm-taste-competition-v2` registered with
   `enabled_toolsets: ["file", "delegate"]` and `deliver: "origin"`.

Without #2 and #3, the cron will exit SILENT every tick (pre-init
state). This is by design — it waits for the parent to finish
setup without spamming.

## First-cycle execution recipe

1. Cron tick fires. Reads queue → `conserver-gi` top.
2. `conserver-gi` (delegate_task, file-only) reads TASTE_SCORE,
   COMPETITION_HARNESS, this cycle brief. Identifies D2 as target.
3. Reads recent ENGINE source (file-only) for likely targets:
   - `Engine/Source/Runtime/Private/Renderer/PathTrace/GIPass.cpp`
     (sky-bounce clamping, light sampling)
   - `Engine/Source/Runtime/Private/Renderer/ReSTIR/ReSTIR.cpp`
     (reservoir sampling, temporal accumulation)
   - `Engine/Source/Runtime/Public/Renderer/GI.h` (interface)
4. Writes `docs/PENDING_BUILD_cycle_0_round_0.md` with proposed
   patch + build/render commands + expected dump path.
5. Cron exits tick.
6. Parent (you) sees the PENDING_BUILD in your next session.
7. You (or delegate_task subagent) apply the patch, run build,
   run render, capture dump.
8. You write `docs/BUILD_RESULT_<id>.md` with status.
9. Next cron tick (or this one if executor was fast): reads
   BUILD_RESULT, dispatches scorer.
10. Scorer writes `docs/SCORES/cycle_0_round_0.md`.
11. Cron re-ranks queue, writes cycle 1 brief. Loop.

## Rollback plan

If the proposed patch breaks the build:
- Parent executor reverts the patch (`git apply -R` or manual).
- Writes "BUILD FAILED" to BUILD_RESULT.
- Cron next tick sees the failure, reverts queue (conserver-gi
  pushed to back), dispatches next profile (conserver-noise).
- No harm done.

If the proposed patch produces a worse frame:
- Scorer writes delta_total < 0.
- Cron re-ranks: conserver-gi demoted 5 points (per
  COMPETITION_HARNESS §7).
- The "winning" frame is whatever was committed BEFORE cycle 0.
  v0 = pre-competition state, v1 = first successful cycle's score.

## Anti-gaming (preserved)

- Reference render is hash-checked at every scorer call.
- Score moves by 0.5 increments.
- The parent executor cannot influence scoring (only the scorer
  writes scores). The cron tick verifies this separation.

---

**This is a planning doc, not a deliverable. The deliverables are
the 3 files in "Expected deliverables" above.**