# docs/archive/

This directory holds old session outputs, debug logs, and historical artifacts that
are **no longer load-bearing** for the project but kept locally for reference.

**What's here:**

- `cron-tick-logs/` — 9156 auto-generated `PIPELINE_HEALTH_*.md` and `OVERSEER_HEALTH_*.md` files from a now-decommissioned cron pipeline. Each file is a single tick log; nothing in them is unique to any one of them. Deleted 2026-09-01 (Phase 3 cleanup).
- `repair-attempts-2026-07-26-to-2026-07-30/` — pre-Phase-1 repair notes. Kept for historical continuity.

**What was here but moved/deleted 2026-09-01:**

- All `PIPELINE_HEALTH_*.md` and `OVERSEER_HEALTH_*.md` files in `docs/` — moved to `archive/cron-tick-logs/` then deleted (no unique content).
- Stray tick logs in `home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/docs/` (bot path bug, identical content) — deleted.

**What is preserved at root of `docs/`:**

- `GOAL_2026-09-01.md` — the active goal doc (this phase)
- `AI_NAVIGATION.md` — agent orientation map
- `TEST_GUIDELINES.md` — assertion policy
- `PHASE_1_REPORT.md`, `PHASE_1_SDD_TDD_AUDIT.md`, `PHASE_2_AI_NATIVE_AUDIT.md`, `PHASE_3_REDUNDANT_FILES_REPORT.md`
- `DIAGNOSTIC_*.md` — incident-specific investigations (one per incident)
- `MATERIAL_CONVENTIONS.md`, `DISPATCHER_PROMPT.md` — load-bearing policy docs

**What lives in `docs/archive/repair-attempts-2026-07-26-to-2026-07-30/`:**

5.2M of pre-Phase-1 repair attempts. Kept because each one documents a real bug it tried to fix.