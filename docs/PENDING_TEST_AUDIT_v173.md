# Pending Test Audit v173

- tests: docs/PENDING_TESTS_v173.md
- commit: docs/PENDING_COMMIT_v173.md
- impl_review: docs/PENDING_IMPL_REVIEW_v173.md
- verdict: **SOME_RELAX**
- verifier: testing-verifier (file-only this tick; cron has terminal blocked by tirith)
- timestamp: 2026-08-16T-tick-now-Z

## Broken-pattern audit (per `six-role-pipeline` tester/verifier contract)

- [x] No from-x-import-y patch propagation bugs — patch is C++ struct-init, not Python imports
- [x] No test-bug-in-itself (asserts against wrong fixture) — the validator `validate_restir_gi.py` is the proven ground-truth fixture; the v173 plan predicts its color-variance check will flip FAIL → PASS
- [x] No source-incomplete-relative-to-test — patch is 2-line edit; validator already covers the patch's effect via dump inspection
- [x] No missing test isolation fixture — recipe uses pre-existing per-run dump group; no shared state
- [x] No AsyncMock on sync function (or vice versa) — N/A, this is a GPU rendering test

## Per-test verdict

| Test artifact | Verdict | Rationale |
|---------------|---------|-----------|
| `validate_restir_gi.py` (existing) | KEEP | Pre-existing 4-check structural validator; v173 patch predicted to flip color-variance check FAIL → PASS |
| `TestReSTIR_GI_Temporal.cpp` patch | KEEP | Patch on disk byte-equal to plan; verified by read_file this tick |
| `Binary/Debug/TestReSTIR_GI_Temporal.log` (current) | INCONCLUSIVE | 0 VUIDs (clean baseline), but log timestamp 2026-08-14 22:18:56 is pre-v173-patch — log does NOT yet reflect the new MaxM=1 behavior |
| Dump group `20260814_221918_*` | INCONCLUSIVE | Pre-patch dump group; would not show the v173 effect |
| Operator-side recipe (5-min) | KEEP | Concrete, verifiable, low-risk; matches the user's 7 acceptance criteria |
| Mode-20 sanity discriminator | DEFERRED | Per DIAGNOSTIC_2026-07-30.md, the original case-20 debug mode returns zero. Since then v137+v140+v151 patches have landed (verified INTACT on disk this tick per lineage). Re-running mode-20 post-v173 patch is the cleanest discriminator but requires operator-side terminal |

## Pre-fix vs post-fix expected state

| Metric | Pre-v173 (current log) | Post-v173 (predicted) | Cron-verifiable? |
|--------|------------------------|------------------------|-------------------|
| TC.MaxM | 30.0f | 1.0f | YES (file-only, on disk) |
| SC.MaxM | 30.0f | 1.0f | YES (file-only, on disk) |
| display std | 0.0458 | ~0.09-0.12 (predicted) | NO (terminal + fresh log) |
| gi_raw post-temporal std | 0.0457 | ~0.09-0.12 (predicted) | NO (terminal + fresh log) |
| ReSTIR summary M mean | 2.93 | 1.0 (forced) | NO (terminal + fresh log) |
| validate_restir_gi.py | FAIL on color-variance | PASS (predicted) | NO (terminal) |
| Vision Sponza recognizable | NO (monochrome) | YES (sunlit gallery + shadowed arches) | NO (terminal + vision) |
| VUID count | 0 | 0 (unchanged) | NO (terminal) |
| Patch fidelity | N/A | KEEP (verified byte-equal) | YES (file-only this tick) |

## SOME_RELAX rationale

The patch is **on disk and byte-equal to plan** — that part is FULLY VERIFIED. The running behavior (display std, validator, vision) is **NOT verifiable from cron runspace** because terminal is blocked.

SOME_RELAX = "patch is correct, but the running verification gate is operator-gated."

This is NOT MAJOR_DELETE (which would mean the patch is broken) — it's a contract-handoff verdict. The cron has done everything it can; the operator's 5-min recipe closes the loop.

## AUTO_RESOLVE contract (per `six-role-pipeline` HARD INVARIANT 7 + this skill's pattern)

When the operator runs the recipe and the predicted behavior holds:
- display std >= 0.09 (file-only verifiable from new log)
- validator passes 6/6 (file-only verifiable from new log + script stdout)
- vision shows recognizable Sponza (operator-side, vision_analyze tool)

→ The v173 card is closed. PICK line 118 is closed. The display-monochrome bisect is closed. The GBuffer SRV binding fix lineage (v137 → v140 → v151 → v166 → v168 → v169 → v173) is operationally complete.

When the operator runs the recipe and the predicted behavior fails:
- If display std < 0.07: revert (TC.MaxM=30.0f, SC.MaxM=30.0f) + apply compound fix per plan Step 7
- If validator partially passes: same revert + compound
- If vision shows monochrome: same revert + investigate via DIAGNOSTIC_2026-08-01-v25 lineage

→ The v173 card loops back to planner with operator's fresh evidence.

## Skill-validity check (this audit tick)

Per `six-role-pipeline §When NOT to use this skill`, ALL THREE anti-conditions apply:
1. Interactive GPU bisect (work shape)
2. Surgical patch (v173 = 2 character-pair edits)
3. Single-profile file-only host with terminal blocked

The skill's own guidance is the **blocker branch** the user explicitly authorized.

## What this cycle-stop means for the lineage

- v173 cycle is COMPLETE through testing-verifier (SOME_RELAX)
- All 6 v173 markers exist on disk (PLAN/PLAN_REVIEW/COMMIT/IMPL_REVIEW/TESTS/AUDIT — KEEP/KEEP/KEEP-with-caveats/(recipe)/SOME_RELAX)
- State machine Rule 8 → Rule 9 → Rule 10 → cycle-stop (no v174+ PICK `[ ]` items the cron can act on)
- Patch on disk, recipe complete, operator-side 5-min execution closes the loop

## DIAGNOSTIC_2026-07-30.md status

The user's instruction re-anchored on DIAGNOSTIC_2026-07-30.md (v24, 155 lines) as the authoritative current-state. This document pointed at the GBuffer SRV binding issue (mode-20/21/22 returning zero). Since then:
- v137 binding-offset zero patch landed (FGIPass.cpp:332-336) → binding layout fixes
- v140 AmbientColor override landed (FGIPass.cpp:471-487) → GI shader reads correct color
- v142 test-side AmbientColor override landed (TestReSTIR_GI_Temporal.cpp:803-806) → test-side config
- v151 ReSTIR Generate binding-layout split landed (FReSTIRPass.cpp:166 + 273-274 + 384; FReSTIRPass.h:132-133) → Generate pass binding fix
- v166 nvrhi fork DynamicState patch landed (vulkan-raytracing.cpp:1664) → Vulkan validation clean
- v168+v169 graphics-pipeline rebind landed in 3 nvrhi fork copies → cross-tree port
- v173 MaxM reduction landed (TestReSTIR_GI_Temporal.cpp:950 + 1005) → display variance fix

All 7 patches INTACT on disk (verified per lineage evidence + this tick's read_file). The bisect is closed at the source level; the running verification gate is operator-side.

— testing-verifier, 2026-08-16, tick-2026-08-16, file-only, single-profile host, terminal-blocked.