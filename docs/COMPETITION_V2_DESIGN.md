# HLVM-Engine Taste-Score Competition — v2 Design (file-only cron)

**Dated 2026-09-02 (evening) — replaces v1 after tirith blocker.**
**Pairs with:** `docs/COMPETITION_HARNESS.md` (v1 rules),
 `docs/TASTE_SCORE.md` (rubric, unchanged).
**Why v2:** v1 cron registered with `enabled_toolsets: ["terminal",
"file", "delegate"]` could not execute shell commands (tirith
deny-all policy on cron subagents). Every tick exited with
`tool_failure: terminal blocked`. v2 moves the seam: cron = file
+ delegate only, parent executor = shell.

---

## 1. The seam

```
            ┌────────────────────┐
            │ COMPETITION_QUEUE  │  ← parent stages; ranks per cycle
            └─────────┬──────────┘
                      │
                      ▼
            ┌────────────────────┐
            │  Cron tick (30m)   │
            │ file + delegate    │
            │ NO terminal        │
            └─────────┬──────────┘
                      │
        ┌─────────────┼─────────────┐
        ▼             ▼             ▼
   ┌────────┐  ┌──────────┐  ┌──────────────┐
   │Conserver│ │  Scorer  │  │   Parent     │
   │proposes │ │ reads    │  │   executor   │
   │PENDING_ │ │  dump +  │  │  (terminal)  │
   │BUILD.md │ │  ref     │  │              │
   └────┬────┘ └────┬─────┘  └──────┬───────┘
        │           │              │
        ▼           ▼              ▼
   docs/PENDING_BUILD_<id>.md  ─── reads ───  runs ───  docs/BUILD_RESULT_<id>.md
                                              │
                                              ▼
                                          docs/SCORES/cycle_<N>_round_0.md
```

The bridge files are the seam:
- `docs/PENDING_BUILD_<id>.md` — proposed diff + commands.
- `docs/BUILD_RESULT_<id>.md` — exit codes + dump path.

The cron **proposes**, the parent **executes**, the cron **scores**.

---

## 2. Per-tick flow (file-only)

1. Read `docs/COMPETITION_QUEUE.md` — pop top profile.
2. Read most recent `docs/SCORES/*.md` (last cycle's verdict).
3. Dispatch a `conserver-*` delegate_task (subagent has the
   generic conserver prompt from `dispatcher_competition.md`).
4. The conserver writes `docs/PENDING_BUILD_<id>.md` with:
   - target file path(s)
   - proposed patch (≤ 200 lines)
   - build command
   - render command
   - expected dump path
5. Cron exits tick. Waits for parent executor.
6. Parent executor (your session or a subagent with terminal) sees
   `PENDING_BUILD_<id>.md`, runs the commands, writes
   `docs/BUILD_RESULT_<id>.md` with exit codes + dump path.
7. Next cron tick (or same tick if executor was fast): read
   `BUILD_RESULT_<id>.md`, dispatch `scorer` delegate_task (file
   only), scorer writes `docs/SCORES/cycle_<N>_round_0.md`.
8. Re-rank queue per `COMPETITION_HARNESS.md §7`. Write
   `docs/COMPETITION_HEALTH_<date>.md` one-liner.

---

## 3. The two bridge files

### `docs/PENDING_BUILD_<id>.md` (conserver writes)

```
# PENDING BUILD <id> — Cycle <N>, Round <M>, Profile <profile-name>

## Target dimension
D2 (light transport)

## Diagnosis (2-3 sentences)
<why this change targets the weakest dimension>

## Proposed patch
- File: <path>:<line>
- Diff:
    -    <old line>
    +    <new line>
- Total lines changed: <N>

## Build command
./Build.sh --Config=Debug --Target=TestPathTraceGI --Test

## Render command
cd Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestPathTraceGI

## Expected dump path
Binary/Debug/dumps/cycle_<N>_round_<M>/TestPathTraceGI.exr

## Risk / rollback
<one paragraph: what could go wrong, how to revert>

## Status
PENDING — waiting for parent executor
```

### `docs/BUILD_RESULT_<id>.md` (parent executor writes)

```
# BUILD RESULT <id> — Cycle <N>, Round <M>

## Build status
- exit_code: <0 / non-zero>
- duration_sec: <N>
- log tail: <last 10 lines>

## Render status
- exit_code: <0 / non-zero>
- duration_sec: <N>

## Dump
- path: <relative path or "MISSING">
- size_bytes: <N>
- sha256: <hash>

## Patch applied
- yes / no / partial
- if no/partial: <reason>

## Status
OK / FAILED — <one-line summary>
```

---

## 4. Parent executor (the shell-side)

Two trigger modes:

### Mode A: Manual (recommended first run)
- You run `./Build.sh && ./Render.sh` once in your interactive
  session when you see a `PENDING_BUILD_*.md`.
- Write `BUILD_RESULT_*.md` yourself.
- **Pros**: zero permission surprises, you see every command.
- **Cons**: requires you to be online.

### Mode B: Auto via delegate_task (try this if Mode A is too slow)
- From your interactive session (which has terminal consent),
  call `delegate_task` with `enabled_toolsets: ["terminal",
  "file"]`.
- The subagent scans `docs/PENDING_BUILD_*.md`, runs the
  commands, writes `BUILD_RESULT_*.md`.
- **Pros**: works while you're offline.
- **Cons**: tirith *might* still block subagents. Test on the
  first cycle.

### Mode C: Second cron attached to a parent-allowed profile (advanced)
- Register a second cron with `attach_to_session: true` on a
  profile that has `terminal` consent.
- This cron fires every 5 min, scans for `PENDING_BUILD_*.md`,
  runs them, writes `BUILD_RESULT_*.md`.
- **Pros**: fully autonomous overnight.
- **Cons**: requires a profile-level config change. Likely won't
  work on this host without host-admin cooperation.

Default: Mode A (manual). Upgrade to Mode B after one successful
cycle. Mode C only if Mode B is blocked.

---

## 5. State machine (v2 cron)

```
state = {
  cycle_id:        monotonic int
  queue:           docs/COMPETITION_QUEUE.md
  pending_build:   docs/PENDING_BUILD_<cycle_N>_round_<M>.md
  build_result:    docs/BUILD_RESULT_<id>.md
  last_score:      docs/SCORES/cycle_<N>_round_<M>.md
  reference_hash:  sha256 of docs/reference_renders/cornell_box_reference.exr
}

# Routing per tick:

1. queue missing → exit SILENT ("cron not initialized")
2. reference render missing → exit SILENT, write
   "pre-init: reference render missing" to health doc
3. active profile from top of queue
4. If no pending_build for current cycle:
   dispatch conserver, write pending_build, exit
5. If pending_build exists AND no build_result AND age > 10 min:
   write "WAITING: parent executor hasn't run yet" to health
   doc, exit (no spam)
6. If build_result exists AND no score for current cycle:
   dispatch scorer (file-only), write score, exit
7. If score exists:
   re-rank queue, bump cycle_id, write next cycle brief, loop to step 3
```

The new rule (step 5) prevents spam: cron no longer writes a
fresh "tool_failure" line every tick while waiting for parent.
It writes one line per cycle, ages out cleanly.

---

## 6. Cron registration (v2)

```json
{
  "name": "hlvm-taste-competition-v2",
  "schedule": "every 30m",
  "enabled_toolsets": ["file", "delegate"],
  "deliver": "origin",
  "attach_to_session": true,
  "workdir": "/home/hangyu5/Documents/Gitrepo-My/HLVM-Engine",
  "prompt": "<contents of docs/agents/dispatcher_competition.md v2>"
}
```

Key change: `terminal` removed from `enabled_toolsets`. The cron
no longer attempts shell — it cannot fail on tirith because it
never tries.

---

## 7. Dispatcher prompt changes (v2)

The `dispatcher_competition.md` v2 must:

1. Never call `terminal` (file + delegate only).
2. Write `PENDING_BUILD_*.md` instead of running build/render.
3. Read `BUILD_RESULT_*.md` (parent executor's output).
4. The scorer remains file-only — reads dumps via file tools, not
   shell.

The skill content from v1 (`docs/COMPETITION_HARNESS.md §3-7`) is
unchanged. The cron prompt rewrite is small (~50 lines diff).

---

## 8. Anti-gaming (preserved from v1)

All §8 anti-gaming rules apply unchanged:
- Reference render frozen, hash-checked.
- Score moves by 0.5 increments.
- 3 reference scenes weighted equally (when staged).
- D3 noise penalty non-linear.
- Scorer is independent of submitter.

New in v2:
- **The parent executor cannot influence scoring.** It only runs
  commands; the score file is the scorer's output, not the
  executor's. This separation is enforced by the prompt contract.

---

## 9. Termination

Same as v1:
- Score plateaus (Δtotal < 0.5 across 3 cycles) → write
  PLATEAU marker, rotate to different scene.
- User can pause/reset anytime via `cronjob action=..."`.
- Parent executor runs at user's discretion.

---

## 10. Self-review

v2 design choices to critic-check before cron re-registration:

1. **Is `PENDING_BUILD_*.md` + `BUILD_RESULT_*.md` the right
   seam?** Alternative: a single `PENDING_BUILD_*.md` updated
   in-place by parent (one file, not two). Pro: less filesystem
   noise. Con: parent must write back to the same path (race
   condition if parent and cron tick overlap). Two files is
   append-only, safer.

2. **Is "wait 10 min before re-pinging" the right cadence?**
   Alternative: poll every tick but write the same "WAITING"
   line. The cron's audit trail prefers fewer lines; 10 min is
   the chosen throttle.

3. **Is Mode A → Mode B → Mode C the right escalation path?**
   Alternative: skip straight to Mode C and accept that it
   might not work. Pro: faster iteration. Con: if Mode C is
   blocked, you lose the work.

4. **Hard cap on cycles before PLATEAU?** v1 had 3 consecutive
   cycles with Δtotal < 0.5. v2 keeps this.

---

## 11. Round report (design phase)

success criteria:
- v2 design written with seam, flow, bridge files, state machine
- Anti-gaming preserved
- Reversibility documented (host config change → v1 re-enabled)

criteria status:
 - v2 design: met (this doc)
 - v2 prompt rewrite: pending (not yet written)
 - v2 cron registration: pending (not yet registered)
 - reference render: pending (parent must stage)

success confidence: 9/10 — design is sound, follows the
six-role pipeline's file-only mode exactly. Risk is that
`delegate_task` from a parent session also gets blocked by
tirith (Mode B failure mode).
failure confidence: 2/10 — v2 is strictly less ambitious than
v1 (no autonomous builds), so the failure mode is "competition
is manual-only," not "competition broken."
goal sticked: yes — v2 preserves the goal (overnight iteration
on taste score) at a smaller scope (manual execution vs auto).
touched: docs/COMPETITION_V2_DESIGN.md (new), JOURNEY.md (3 new
sections appended)
not touched: docs/COMPETITION_HARNESS.md, docs/TASTE_SCORE.md,
docs/agents/* (intact for reuse)
test ran: none yet
journey: 3 new sections in JOURNEY.md (tirith block, v2 design,
cron kill)
next: rewrite docs/agents/dispatcher_competition.md to v2 spec,
or pivot to single-subagent-in-current-session if user prefers
self review status: 0 critic rounds on v2 (will run 1 round
before cron re-registration)
next step status: wait-for-user — design is ready, user picks
the path forward (rewrite + recreate cron vs single subagent).