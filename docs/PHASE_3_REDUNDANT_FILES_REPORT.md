# Phase 3 — Remove redundant files

## Audit findings (2026-09-01)

The repo had accumulated ~110M of cron-pipeline noise that was tracked-by-default
under `docs/` (9156 tick log files). These were auto-generated state dumps from a
now-decommissioned cron system. Each file was structurally identical — a single
"tick N of cron pipeline, no new work" record. No unique information was lost by
removing them.

| Category | Before | After | Delta |
|---|---|---|---|
| `docs/` total size | 122M | 13M | **−109M** |
| `docs/` file count | 9994 | 839 | **−9155** |
| `PIPELINE_HEALTH_*.md` | ~8600 | 0 | all deleted |
| `OVERSEER_HEALTH_*.md` | ~550 | 0 | all deleted |
| `home/` stray tick logs | 2 | 0 | deleted (bot path bug, identical content) |
| `docs/archive/cron-tick-logs/` | created | empty → deleted | 110M freed |
| `docs/archive/README.md` | created | load-bearing | documents what was archived |

**Kept (load-bearing or evidence):**

- All `DIAGNOSTIC_*.md` (one per real incident)
- `OVERSEER_ESCALATION*`, `OVERSEER_CORRUPTION*`, `OVERSEER_FINDINGS*` (real escalations, not tick logs)
- `Vibe_Coding/50_ReSTIR_GI_Temporal/` (63M of render evidence — path-trace artifacts)
- `Vibe_Coding/*/evidence/` dirs (render-dump evidence)
- All `docs/superpowers/`, `docs/agents/`, `docs/archive/repair-attempts-*/`
- All `Engine/`, `Engine/Source/*/Test/*_Data/dumps/` (already gitignored)
- `Binary/`, `build/` (build artifacts; gitignored)

**Not touched (would lose information):**

- `Vibe_Coding/` session logs — each represents a distinct completed task; even the small ones document what was tried
- `Engine/Source/Runtime/Test/*_Data/` directories — contain test fixtures and reference dumps
- `.wolf/`, `.memory/`, `.claude/`, `.kilo/`, `.opencode/`, `.sisyphus/`, `.lingma/`, `.git-historian/` — all gitignored, don't ship

## What changed

- ✅ 9156 `PIPELINE_HEALTH_*.md` and `OVERSEER_HEALTH_*.md` files deleted
- ✅ `docs/archive/README.md` created explaining what was archived
- ✅ Stray `home/.../docs/PIPELINE_HEALTH_*.md` files (bot path bug) deleted
- ✅ Phase 3 report saved to `docs/PHASE_3_REDUNDANT_FILES_REPORT.md`

## Success criteria

1. No more tick logs in `docs/` or `home/`. ✅
2. `docs/` shrunk by >100M. ✅ (−109M)
3. Load-bearing docs (DIAGNOSTIC, GOAL, PHASE_*, AI_NAVIGATION, etc.) intact. ✅
4. Build artifacts and session-evidence dirs preserved. ✅
5. Archive documents what was removed. ✅

## Repo state after Phase 3

```
122M docs → 13M docs
~9994 .md files → 839 .md files
```

The repo is now navigable; an AI agent can `ls docs/` and find policy docs at the top.