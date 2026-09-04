# Competition Health — 2026-09-02

## Tick dispatch failures

Cron was created with `enabled_toolsets: terminal, file`. State at tick start:

- ✅ `docs/COMPETITION_QUEUE.md` present (staged by parent on 2026-09-02).
- ✅ `docs/TASTE_SCORE.md` present (rubric).
- ✅ `docs/COMPETITION_HARNESS.md` not yet verified present (queue references it; not read this tick — terminal blocked).
- ❌ `docs/reference_renders/cornell_box_reference.exr` MISSING (zero `.exr` files anywhere under `docs/`).
- ❌ `docs/SCORES/` MISSING.
- ❌ `docs/COMPETITION_CHANGES/` MISSING.
- ❌ `docs/COMPETITION_CYCLE_<N>.md` not yet authored (parent has not initialized cycle 0 brief).

## Toolset failure

tool_failure: terminal blocked

Every shell invocation in this tick was rejected by tirith (security scan
rejected `ls`, `date`, even basic `ls -la`). The competition's build /
render / dump-capture pipeline (`./Build.sh --Config=Debug
--Target=TestPathTraceGI --Test` →
`cd Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestPathTraceGI`)
is unverifiable without `terminal`. Per dispatcher hard rule §7 the tick
must abort cleanly.

## What was verified without terminal

- File reads succeeded (`docs/agents/dispatcher_competition.md`,
  `docs/COMPETITION_QUEUE.md`, `docs/TASTE_SCORE.md`).
- File search succeeded (zero `.exr` files under `docs/`).
- File write succeeded (this file).

## What was NOT verified

- Hash of `docs/reference_renders/cornell_box_reference.exr` —
  file does not exist. Per HARNESS §1 the competition cannot start
  without a frozen reference. Parent session must `git add` /
  stage the EXR before any cron tick is productive.
- Queue freshness — `docs/COMPETITION_QUEUE.md` was last updated
  2026-09-02 (parent-staged initial order), consistent with the
  dispatcher's initial stage.

## Recommended parent actions

1. Stage `docs/reference_renders/cornell_box_reference.exr` (commit
   hash-checked; do NOT modify after).
2. Pre-create `docs/SCORES/` and `docs/COMPETITION_CHANGES/`.
3. Confirm cron host has an approved shell-sandbox policy for
   `./Build.sh` + the TestPathTraceGI binary. The current tirith
   pattern_blocks all shell commands, including no-op `ls`, which
   suggests the cron was registered with a toolsets restriction
   the sandbox parser doesn't recognize.
4. Until both blockers clear, cron will exit with this same message
   every tick. Consider `_SILENT` mode for ticks where the gate is
   unmet (write this file only every Nth tick or on state delta).

## Health line (per rule §6 single-line format)

<2026-09-02T??:??:??+08:00> cycle0 | active=conserver-gi | route=exit | score=0/100 | delta=0
tool_failure: terminal blocked; reference render missing; no cycle 0 doc authored yet
