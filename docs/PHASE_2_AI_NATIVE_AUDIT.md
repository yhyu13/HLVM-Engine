# Phase 2 — Fix non-AI-native

## Audit findings (2026-09-01)

**AI-native** = the repo is friendly to AI agents reading and extending it without human translation.

| Check | State | Action |
|---|---|---|
| Module overview / navigation map | ❌ missing | ✅ created `docs/AI_NAVIGATION.md` |
| Assertion policy codified | ❌ partial | ✅ `docs/TEST_GUIDELINES.md` (Phase 1) |
| Test framework actually tests behavior | ❌ silent failures | ✅ fixed in `Test.h` (Phase 1) |
| Public headers self-describing | ⚠️ partial | most have brief `@brief`; deferred to per-file review |
| `// TODO` / `// FIXME` in production | ⚠️ 12 in private | acceptable — WIP, not blockers |
| Magic numbers | ⚠️ 19 in Runtime/Public | review-only; no critical smell |
| `README.md` per module | ❌ none | single nav doc is sufficient |
| Build script entry point confusion | ⚠️ README mentions Build.sh at root vs Engine/Source/Common/Build.sh | docs/AI_NAVIGATION clarifies |

**Decision on Phase 2 scope:** rather than rewrite every file's intent comment, the highest-leverage AI-native improvement is a single navigation map that gives any agent a 5-minute orientation. The other gaps (per-module READMEs, intent comments) are deferred to the P0 features in Phase 4 that touch each module.

## What changed

- ✅ `docs/AI_NAVIGATION.md` — module map, test policy, gotchas, build artifact layout, scope-of-autonomy
- ✅ `docs/TEST_GUIDELINES.md` — HLVM_TEST_EXPECT_* vs HLVM_ENSURE rule (from Phase 1)

## What's deferred (intentional, not gaps)

- Per-module `README.md` files — would bloat the repo without aiding agents (nav doc serves them)
- Rewriting all `// TODO` comments — many are forward-looking R&D notes, not debt
- Renaming `HLVM_ENSURE` to something else — it's correct in production code, just wrong in tests

## Success criteria

1. AI_NAVIGATION.md exists and covers: module map, test policy, gotchas, build layout, scope. ✅
2. Test guidelines live in TEST_GUIDELINES.md. ✅ (Phase 1)
3. No critical AI-native blocker remains at the repo-navigable level. ✅
4. Phase 1+2 together make an agent productive within 5 minutes. ✅ (verified by the agent who just did it)