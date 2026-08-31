# Pending Plan v179

- task: Apply v176 patch (wire CVar + env-var hook) and report concrete terminal-blocked blocker
- source: docs/PENDING_COMMIT_v176.md (KEEP'd at tick-85, plan-criticer KEEP at tick-83)
- approach: Apply 4-edit v176 patch to `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` (Edit 1: add `Renderer/GI/GICVars.h` include at line 56; Edit 2: replace `TC.MaxM = 1.0f` with `CVar_r_ReSTIR_MaxM.GetValue()` at line 966; Edit 3: replace `SC.MaxM = 1.0f` with `CVar_r_ReSTIR_MaxM.GetValue()` at line 1021; Edit 4: add inline HLVM_RGI_MAXM env-var hook at line 625). Verify patch integrity via file-only search. Report concrete external blocker (tirith terminal denial) for the build/run/validate/vision/mode-20 acceptance gates. Do NOT fabricate any build/test/vision result.
- diff_estimate: +16 / -2 = +14 lines in TestReSTIR_GI_Temporal.cpp (4 include + 2 CVar reads (same line count) + 14 env-var hook - 2 hardcoded `1.0f` = +16/-2)
- skip_plan_review: no — the patch touches multi-instance CVar architecture and the env-var plumbing must be verified; reviewer recommended
- test_strategy: 0 new tests (the recipe itself IS the test — apply patch, run binary, grep log, validate, vision-check). The cron cannot run the recipe; the operator runs it.
- risks:
  - **Risk 1: compile error from the patch** — manually verified: include path is correct (GICVars.h:38 declares `AUTO_CVAR_FLOAT(r_ReSTIR_MaxM, 30.0f, ...)`); `GetValue()` returns `float` matching `TFP32 MaxM`; `SetValue()` exists per `CVarMacros.h:71`; `try/catch` pattern matches existing `HLVM_RGI_EXPOSURE` hook (line 605-609). Brace-matching verified at lines 627-638. No new member added (inline shape per v176 commit's "Impler choice — both are within v176 scope. Recommended: inline shape").
  - **Risk 2: env-var hook fires before test members are set** — verified: the hook is at line 625, after `bBypass` and after the `HLVM_RGI_EXPOSURE` hook (line 605). It is INSIDE the `if (bDumpRequested) {...}` block? No — it is OUTSIDE, at the same level as `bBypass` set. The CVar `SetValue` is global, so it persists across the test instance. Safe.
  - **Risk 3: cron cannot verify build** — ACCEPTED. Terminal access is blocked by tirith (95th consecutive tick). The operator runs `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild` to verify the build.

## Cycle lineage (this card)

- v176 cycle (2026-08-17 ticks 82-87): plan/commit/impl-review/tests/audit all KEEP/ALL_KEEP, patch on disk unapplied, terminal-blocked, operator-gated.
- v177 cycle (2026-08-18 ticks 88-92): heartbeat re-asserting v176 closure path. +0 lines, 0 tests. ALL_KEEP.
- v178 cycle (2026-08-18 tick-93): heartbeat recommending cron pause. +0 lines, 0 tests, 1 new finding. ALL_KEEP.
- **v179 cycle (THIS TICK, tick-now-94)**: apply v176 patch from cron runspace (file-only tools), stage commit marker, then report concrete external blocker for the build/run/validate gates.

## Acceptance status (per cron prompt)

| # | Criterion | Status |
|---|-----------|--------|
| 1 | Debug target builds | **BLOCKED** — terminal access denied by tirith (95th consecutive tick) |
| 2 | HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 | **BLOCKED** — terminal access denied |
| 3 | No Vulkan VUID/ERROR | **BLOCKED** — requires log grep via terminal |
| 4 | No command-list errors | **BLOCKED** — requires log grep via terminal |
| 5 | validate_restir_gi.py passes | **BLOCKED** — requires python3 via terminal |
| 6 | vision: recognizable Sponza | **BLOCKED** — requires vision_analyze on fresh dump, dump requires terminal |
| 7 | HLVM_PT_DEBUG_MODE=20 returns non-zero | **BLOCKED** — requires terminal + re-run |

**7/7 acceptance gates BLOCKED by tirith terminal-denial policy.** The patch application (the only file-only step) IS done.

## What was done this tick (file-only, audit-trail-honest)

1. Applied Edit 1 (line 56): `#include "Renderer/GI/GICVars.h"` added
2. Applied Edit 2 (line 966): `TC.MaxM = CVar_r_ReSTIR_MaxM.GetValue()`
3. Applied Edit 3 (line 1021): `SC.MaxM = CVar_r_ReSTIR_MaxM.GetValue()`
4. Applied Edit 4 (line 625-638): HLVM_RGI_MAXM env-var hook (inline shape, no new member)
5. Re-verified patch integrity via 4 search_files passes (v176 markers at 4 expected locations, no syntax warnings, brace-matching balanced)

## RECOMMENDATION (carry forward from v178)

Pause the six-role-pipeline cron. The pipeline has converged: v176 patch is now APPLIED. The 5-min recipe (rebuild + run + grep + validate + vision + mode-20) is the operator's next step. The cron cannot run the recipe (terminal blocked). Continuing to heartbeat will produce v180, v181, ... with the same conclusion.

— planner, dispatch from tick-now-94, 2026-08-18, file-only, single-profile host, terminal-blocked, autonomous invocation #34.
