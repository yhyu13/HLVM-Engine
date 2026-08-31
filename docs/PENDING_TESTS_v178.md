# Pending Tests v178

- commit: docs/PENDING_COMMIT_v178.md
- commit_verdict: heartbeat, +0 net lines, skip_impl_review=yes per HARD INVARIANT #2
- test_strategy: **0 new tests.** v178 is a heartbeat. The 7 v176 operator-side scenarios (inherited by v177) remain the closure-gate test surface.
- test_files: (none — v178 produces no code change, no test files)
- scenarios: 7 scenarios inherited verbatim from v176 (and v177)

## Re-verification this tick

| Check | Method | Result |
|---|---|---|
| v176 cycle still CLOSED at ALL_KEEP | read `docs/PENDING_TEST_AUDIT_v176.md` | ✅ on disk, ALL_KEEP |
| v177 cycle still CLOSED at ALL_KEEP | read `docs/PENDING_TEST_AUDIT_v177.md` | ✅ on disk, ALL_KEEP |
| v173 patch INTACT | read `TestReSTIR_GI_Temporal.cpp:950, 1005` | ✅ `TC.MaxM = 1.0f;` + `SC.MaxM = 1.0f;` |
| v176 patch UNAPPLIED | grep `Renderer/GI/GICVars.h` in TestReSTIR_GI_Temporal.cpp | ✅ 0 hits |
| v176 patch UNAPPLIED | grep `HLVM_RGI_MAXM` in TestReSTIR_GI_Temporal.cpp | ✅ 0 hits |
| v140 AmbientColor override IS applied | grep `AmbientColorPtr = Desc.AmbientColor` in FGIPass.cpp | ✅ 1 hit |
| CVar target exists | grep `r_ReSTIR_MaxM` in GICVars.h | ✅ 1 hit |
| Sibling CVar pattern (TestCornellBoxGI) | grep `CVar_r_ReSTIR_MaxM.GetValue()` in TestCornellBoxGI.cpp | ✅ 2 hits |
| v176-recipe.sh exists | file search | ✅ exists |
| validate_restir_gi.py exists | file search | ✅ exists |
| dump_pixelstats.py exists | file search | ✅ exists |
| No fresh operator activity (dumps) | grep `2026081[5-9]*` and `2026082*` in dumps | ✅ 0 hits |
| No fresh operator activity (logs) | grep `2026081[5-9]*` and `2026082*` in logs | ✅ 0 hits |
| Freshest dump group is `20260814_221918_*` | directory listing | ✅ confirmed |
| Freshest log is `TestReSTIR_GI_Temporal.log` 2026-08-14 22:19:18 | file search | ✅ confirmed |

**15/15 source-side checks PASS.** v178 heartbeat is consistent with prior state.

## Inherited scenarios (from v176, unchanged by v177 + v178)

The closure-gate test surface is the 7 v176 operator-side scenarios. These are documented in `docs/PENDING_TESTS_v176.md` §"Scenarios" and re-listed below for the v178 audit:

1. **Build succeeds**: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild` exits 0
2. **No VUID/ERROR in log**: `grep -E "VUID|ERROR|VK_ERROR" TestReSTIR_GI_Temporal.log` returns 0 hits
3. **No command-list errors**: `grep -E "CommandList|validation" TestReSTIR_GI_Temporal.log` returns 0 hits
4. **`HLVM_RGI_MAXM override` log line fires**: `grep "HLVM_RGI_MAXM override" TestReSTIR_GI_Temporal.log` returns 1+ hits
5. **`stats display floats` line shows std ≥ 0.09**: `grep "stats display floats" TestReSTIR_GI_Temporal.log` shows pre-temporal std ≥ 0.09
6. **`validate_restir_gi.py` exits 0 with 6/6 PASS**: `python3 validate_restir_gi.py` on the newest dump group shows 6/6 PASS
7. **`HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial**: log line `stats gbuffer material floats` shows mean GBufferMaterial > 0 (not the v137-era sentinel)

Plus 1 vision check (operator-side, requires opening the display PNG):
8. **Vision check**: the display PNG (newest dump group's `display_frame8.png`) shows recognizable Sponza with sane exposure (NOT monochrome red/black/gray)

## NEW finding this tick

**Recommend pausing the six-role-pipeline cron.** The pipeline has produced the same conclusion for 2 consecutive cycles (v176 + v177) with 0 net new code lines, 0 new test files, and 22+ consecutive ticks of operator silence. Continuing the heartbeat loop consumes cycles with 0 forward progress. The 5-min v176-recipe.sh closure gate is operator-side and cannot be executed by the cron. The pause is conservative and reversible.

This is a meta-finding about the pipeline's health, not a code/test finding. It surfaces in the v178 audit (next tick) and is the operator's call to act on.

## Operator action options (carried forward from v176 + v177)

1. **Run the v176 recipe** (~6 min): apply 4 edits, build, run with `HLVM_RGI_MAXM=1.0 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8`, grep for `HLVM_RGI_MAXM override` log line, grep for `stats display floats` line, run `validate_restir_gi.py`, vision-check display PNG, run with `HLVM_PT_DEBUG_MODE=20`. PASS closes at v176 with cleaner code AND env-var rollback.
2. **Run the v173 recipe** (~5 min): just rebuild with the existing v173 hardcode, run. PASS closes at v173. The 5-min recipe is the same as v176 but without the CVar wiring.
3. **Triangulate v173 + v176** (~11 min): see which path works. If v173 PASS and v176 FAIL: multi-instance CVar broke the env-var path. If v173 FAIL and v176 PASS: the CVar path is the only working path.
4. **Switch to interactive debugging** (per the skill's "When NOT to use this skill" — this is interactive GPU bisect on a single-profile file-only host with terminal blocked).
5. **Pause the cron** (`cronjob action="pause"` on the six-role pipeline cron). The cron will not self-pause. **NEW: this is the recommended action in v178.**
6. **Mark the PICK card `[x]`** without running the recipe: this signals "v176 cycle is closed at the file-marker level; operator is taking responsibility for the next step outside the pipeline." Honest and audit-trail-preserving.
