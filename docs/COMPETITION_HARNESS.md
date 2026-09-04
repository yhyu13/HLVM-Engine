# HLVM-Engine Taste-Score Competition — Harness

**Dated 2026-09-02 — Phase 4d of the four-phase autonomous run.**
**Pairs with:** `docs/TASTE_SCORE.md` (the rubric).
**Purpose:** The rules of the overnight game. Multiple agent
profiles render frames, score them against the rubric, identify the
weakest dimension, and submit a single winning change per cycle.
The score goes up, the user wakes up to a better engine.

---

## 1. Game design in one paragraph

Three (or more) agent profiles — `conserver-pbr`,
`conserver-noise`, `conserver-gi` — each specializes in one of the
three heavy dimensions (D1 PBR, D3 signal/noise, D2 light
transport). Every 30 minutes, the dispatcher picks **one** profile
per cycle. That profile gets the current frame dump + the previous
score breakdown + a budget of N lines of diff + an SPP cap. It
submits ONE change. A separate scorer profile reads the resulting
dump, scores it independently, and the score is recorded. The
dispatcher then picks the next profile based on a *priority queue
that prefers the weakest dimension*. Overnight, this converges —
each cycle targets the weakest link, scores climb monotonically,
and the morning summary shows the deltas per dimension.

The game is **non-cooperative within a cycle** (one profile acts
per cycle) and **cooperative across cycles** (the shared goal is
the total score, not individual profile prestige). A profile that
submits a bad change pays a cost: its priority gets demoted, and
the dispatcher rotates to the next profile sooner.

---

## 2. Roles (agent profiles)

The host must register these profiles BEFORE creating the cron.
This harness assumes:

- **`conserver-pbr`** — specialist in D1 PBR correctness. Has
  access to the BRDF / material / TBN / gamma / IBL shaders and
  knows Cycles ground-truth behavior.
- **`conserver-noise`** — specialist in D3 signal/noise. Has access
  to denoiser / accumulator / ReSTIR / temporal-blend code.
- **`conserver-gi`** — specialist in D2 light transport. Has
  access to path tracer / ReSTIR GI / sky-bounce / light sampling.
- **`conserver-mat`** — specialist in D5 material fidelity. Has
  access to texture pipeline / normal maps / mip selection.
- **`scorer`** — INDEPENDENT profile, no write tools, reads dumps,
  emits scores. Cannot be one of the conservers.
- **`dispatcher`** — the orchestrator profile (different from any
  conserver or scorer). Routes cycles.

If the host only supports one worker profile (this is the case in
many setups), the *conserver* profiles collapse to "same head with
different prompt text." The harness still works, but the
"specialist" claim is weaker — the prompt text drives the focus.
The `scorer` MUST still be a different profile (or a strict
no-write-tolerance prompt) to prevent self-scoring.

---

## 3. State machine (per cycle)

Each cycle is a 30-minute tick. The state machine:

```
state = {
  cycle_id:           <int, monotonic>
  round_id:           <int, monotonic per cycle>
  queue:              docs/COMPETITION_QUEUE.md (priority order of profiles)
  last_score:         docs/SCORES/cycle_<N>_round_<M>.md
  pending_change:     docs/COMPETITION_CHANGES/cycle_<N>_round_<M>.md (conserver's submission)
  reference_render:   docs/reference_renders/<scene>_reference.exr (frozen)
  dump_path:          Binary/Debug/dumps/cycle_<N>/*.exr
}

# Routing rules (first match wins):

# 1. No queue → exit SILENT (cron must be initialized by parent)
if queue is None: exit SILENT

# 2. Last cycle has a pending change that hasn't been scored
if pending_change exists and last_score is None:
    route → scorer

# 3. Last cycle was scored, no new change yet
if last_score exists:
    # Re-rank queue based on last score's weakest dimension
    update queue (push winning conserver's profile to back, rotate)
    route → top of queue (one conserver)

# 4. Conserver submitted, but previous score is too old (> 2 cycles)
# → rerun the render first to get a fresh dump before scoring
if last_score is older than 2 cycles:
    route → render-only agent (replay last winning change, get fresh dump)
    # After dump lands, route to scorer.

# 5. Everything current → exit SILENT (cron tick done early)
```

The state machine is intentionally simpler than the six-role
pipeline (no plan/impl/review/audit gates) because taste-score
iteration is *fast* — small diffs, fast renders, short cycles. The
six-role pipeline is reserved for engineering work with
verifiable contracts (per
`docs/PIPELINE_HEALTH_<date>.md` long-running entries).

---

## 4. Cycle budget per tick

Each tick = 30 minutes wall clock. Per cycle:

- **Render:** ≤ 10 min. Use the existing
  `./Build.sh --Config=Debug --Target=TestPathTraceGI --Test`
  pipeline. SPP cap = 32 (lower is better; agent that scores well
  at 8 SPP with denoiser wins D3).
- **Change:** ≤ 200 lines of diff. Conserver writes the diff to
  `docs/COMPETITION_CHANGES/cycle_<N>_round_<M>.md`, applies it
  via `patch`, builds, runs the test, dumps the frame.
- **Score:** ≤ 5 min. Scorer reads the dump + reference, writes
  the score file.
- **Total:** 15 min ideal, 30 min hard cap. If a tick overruns,
  the dispatcher writes a TIMEOUT marker and exits.

---

## 5. The change format (conserver submission)

Each conserver writes a change to
`docs/COMPETITION_CHANGES/cycle_<N>_round_<M>.md`:

```
# Cycle <N>, Round <M>, Profile <profile-name>

## Target dimension
D2 (light transport)

## Diagnosis (2-3 sentences)
The current frame has D2 = 6/10 because the sky-bounce contribution
on shadow-side surfaces is clamped to 0 (regression of v236 fix).
Logs confirm: HLVM_RGI_ACCUM=8 still yields uniform gray indirect.

## Change
- File: Engine/Source/Runtime/Private/Renderer/PathTrace/GIPass.cpp:341
- Diff:
    -    indirect_sky_contribution = saturate(indirect_sky_contribution);
    +    indirect_sky_contribution = max(0.0f, indirect_sky_contribution);
- Reason: saturation clamp kills low-energy sky bounces, which is the
  valid-domain gate from the v236 fix.

## Verification
- Build: ./Build.sh --Config=Debug --Target=TestPathTraceGI
- Render: cd Binary/Debug && HLVM_DUMP_RGI=1 ./TestPathTraceGI
- Dump path: Binary/Debug/dumps/cycle_<N>_round_<M>_before.exr
- Score file (after scorer): docs/SCORES/cycle_<N>_round_<M>.md

## Risk
If the un-clamped value goes negative (NAN upstream), could cause
firefly. Mitigation: max(0, x) instead of saturate(x) = min(max(0,x), 1)
preserves the floor but kills the ceiling.
```

---

## 6. The score file (scorer output)

```
# Cycle <N>, Round <M> — Score Breakdown

| Dimension | Weight | Score | Delta vs last round | Reason |
|-----------|--------|-------|---------------------|--------|
| D1 PBR    | 1.5    | 7.0   | +0.5               | Albedo OK; roughness curve matches Cycles ref within 5% on chrome sphere |
| D2 Light  | 2.0    | 8.5   | +2.5               | Sky-bounce now visible on shadow-side; indirect = 45% of direct (was 0%) |
| D3 Noise  | 1.5    | 6.0   |  0                 | σ = 0.04 (unchanged); no change in this cycle |
| D4 Comp   | 0.5    | 5.0   |  0                 | Camera unchanged |
| D5 Mat    | 1.0    | 6.0   |  0                 | Materials unchanged |
| D6 Temp   | 1.0    | 6.5   |  0                 | Temporal unchanged |
|-----------|--------|-------|---------------------|--------|
| TOTAL     | 7.5    | 7.0/10 → 70/100 | +6.5 vs last cycle | D2 lift dominates |
```

Each row's `Reason` must cite ONE observable fact from the dump
(pixel count, σ value, ratio) — not subjective language. This is
what makes scores reproducible.

---

## 7. Queue ranking algorithm

After each scored cycle, the queue reorders:

```
priority_score = (
    # Did this profile just improve the score?
    + 10 if delta_total > 0
    + 0  if delta_total == 0
    - 5  if delta_total < 0

    # Did this profile improve its target dimension?
    + 5  if delta_target_dim > 0

    # Did this profile break another dimension?
    - 10 if any other dimension regressed by > 1.0
)

queue.sort_by(priority_score, desc)
```

If a profile's submission regressed a non-target dimension, the
revert is automatic: the dispatcher reverts the change at the
start of the next cycle and the next profile in the queue acts.

---

## 8. Anti-gaming (continued from TASTE_SCORE §6)

Additional rules:

1. **The queue itself is part of the game.** Profiles can see the
   queue order in `docs/COMPETITION_QUEUE.md` and know which
   dimension is currently weakest. They cannot see *future* queue
   state, so they can't strategically pick an easy dimension.
2. **Scorer is blind to submitter.** Scorer reads `cycle_id` and
   `round_id` only. The profile name is hidden until after the
   score is written.
3. **Reference renders are checked at score time.** Scorer
   computes `sha256(reference_render.exr)` and compares to the
   committed hash. If mismatch, score is void and dispatcher
   writes `OVERSEER_ESCALATION.md`.
4. **A profile that submits twice without scoring is paused for
   one cycle.** Prevents flooding.

---

## 9. What the cron needs

A `cronjob` with:
- `schedule`: `every 30m`
- `prompt`: the dispatcher text (see
  `docs/agents/dispatcher_competition.md` — v2 file-only)
- `enabled_toolsets`: `["file", "delegate"]` — **NEVER include
  `terminal`**. v2 design: cron proposes, parent executes.
- `deliver`: `origin` (must be explicit, NOT `local`)

Profiles required on host:
- `conserver-pbr`, `conserver-noise`, `conserver-gi`, `conserver-mat`
- `scorer` (file-only)
- (dispatcher is the cron itself; **parent executor is a separate
  role that runs in your interactive session or via `delegate_task`
  with terminal access**)

If host has only one worker profile, conservers collapse. `scorer`
can be implemented as a different prompt to the same worker profile
**with `enabled_toolsets: ["file"]` only** (no patch / write to
source code). Parent executor still needs `terminal` — that's the
one place shell access is required, and it lives outside the cron.

---

## 9a. Bridge files (v2 — the seam)

The cron cannot run shell (tirith blocks). v2 introduces two
bridge files that cross the seam between file-only cron and
shell-allowed parent executor.

### `docs/PENDING_BUILD_cycle_<N>_round_<M>.md`

**Written by:** the conserver (cron-dispatched).
**Read by:** the parent executor.

```
# PENDING BUILD cycle<N> round<M> — Profile <name>

## Target dimension
D<x>

## Diagnosis
<2-3 sentences>

## Proposed patch
- File: <path>:<line>
- Diff:
    -    <old>
    +    <new>
- Total lines changed: <N>

## Build command
./Build.sh --Config=Debug --Target=TestPathTraceGI --Test

## Render command
cd Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestPathTraceGI

## Expected dump path
Binary/Debug/dumps/cycle<N>_round<M>/TestPathTraceGI.exr

## Risk / rollback
<one paragraph>

## Status
PENDING — waiting for parent executor
```

### `docs/BUILD_RESULT_<id>.md`

**Written by:** the parent executor.
**Read by:** the cron (next tick) and the scorer.

```
# BUILD RESULT <id> — Cycle <N>, Round <M>

## Build status
- exit_code: <0 / non-zero>
- duration_sec: <N>
- log_tail: <last 10 lines>

## Render status
- exit_code: <0 / non-zero>
- duration_sec: <N>

## Dump
- path: <path or "MISSING">
- size_bytes: <N>
- sha256: <hash>

## Patch applied
- yes / no / partial
- reason if not: <reason>

## Status
OK / FAILED — <one-line summary>
```

The cron tick waits up to 10 min for `BUILD_RESULT_*.md` after
writing `PENDING_BUILD_*.md`. After 10 min it writes one
"WAITING" line and exits. This throttles spam from 1 line/tick
(30/min) to 1 line/cycle (~30/hr). See
`docs/agents/dispatcher_competition.md §"Anti-spam rules"`.

---

## 10. Termination

The cron runs indefinitely. The user can:
- **Pause** with `cronjob action="pause"` (e.g., before a manual
  edit session).
- **Inspect** the morning score in chat (delivered `origin`).
- **Reset** by deleting `docs/COMPETITION_QUEUE.md` and the
  `docs/SCORES/` directory.
- **Switch the active scene** by editing
  `docs/COMPETITION_QUEUE.md` (the queue can hold per-scene
  entries).

The game ends when the user says it ends, or when the score
plateaus (Δtotal < 0.5 across 3 consecutive cycles — the
dispatcher writes a "PLATEAU" marker and rotates to a different
scene).

---

## 11. Round report (planning)

success criteria:
- TASTE_SCORE.md written with 6 dimensions, weights, formulas
- COMPETITION_HARNESS.md written with roles, state machine, queue ranking
- Cron registered with dispatcher prompt

criteria status:
  - Rubric written: met
  - Harness written: met
  - Cron registered: pending

success confidence: 8/10 — design is clear, follows AMG / six-role
patterns, but the actual overnight behavior is unproven.
failure confidence: 2/10 — main risk is single-profile collapse
reducing "specialist" to "same head with different prompt"; the
harness still works in that mode.
goal sticked: this is part of Phase 4d — taste score + competition
harness; user explicitly requested it.
touched: docs/TASTE_SCORE.md (new), docs/COMPETITION_HARNESS.md (new)
not touched: docs/FEATURE_ROADMAP_2026-09-01.md (100 tasks intact),
docs/P0_PLAN.md (P0 plan intact), Engine/ source code (untouched)
test ran: none yet (cron will run them)
journey: round 1 only; JOURNEY.md not yet updated
next: write the dispatcher prompt + register the cron
self review status: 0 critic rounds on the harness (will run 1
round before cron starts)
next step status: auto-start