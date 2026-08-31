# Pending Plan Review v181

- plan: docs/PENDING_PLAN_v181.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (file-only tick-now-490)
- timestamp: 2026-08-29

## Design soundness

The v181 plan is the file-only execution of the patch that v180 staged but did not implement. Three observations:

1. **Patch scope is the exact 3 RELAX items from v180 audit.** v181 lines 5-12 list the 8 patch sub-steps (add state vars, add argparse cases, widen gate-5/7 classifier, add new diagnosis messages, add gate 8, add gate 9, extend help text, update the staged patch doc). All map 1:1 to the 3 RELAX items in `PENDING_TEST_AUDIT_v180.md:30-46` (BLUE-MID envelope, gray-mid envelope, --mode-30 flag). No scope creep, no additional surface area.

2. **No source code touched.** The plan's "Files touched this cycle" (lines 54-58) is explicit: only `v176-recipe.sh` (the recipe script) and 7 marker files. FGIPass.cpp / GIPathTracing.hlsl / TestReSTIR_GI_Temporal.cpp all source-frozen per v180 risk #3 and v181 carries that forward. Plan-critique honors HARD INVARIANT 2 of v180 audit (test files always trigger the reviewer — `produces_test_files: no` correctly applied).

3. **`skip_plan_review: yes` + `skip_impl_review: yes` are correct.** The plan is a wrap-up of v180 SOME_RELAX items; the v180 plan + audit already authorized this exact scope. The cycle cost is just plan → impler → tester (3 ticks, not the full 6-role cycle), which matches the AMM/Six-role pattern for surgical recipe extensions.

## Plan completeness

- **File-only verification of v180 patch + scaffold state**: PASS this turn (re-verified — `v176-recipe.sh` on disk, 312 lines pre-edit; `validate_restir_gi.py` on disk; 6/6 agent role files; `DISPATCHER_PROMPT.md` on disk).
- **Recipe bash syntax**: balanced (17 if / 17 fi, 3 case / 3 esac, 2 for / 2 done — verified this turn by file-only grep; the 3 extra `if`s in counts are Python heredoc conditionals, not bash).
- **Mode-31 source verification**: Confirmed case 31u at `GIPathTracing.hlsl:782-791` (per v180 plan line 4); the alive-sentinel reads `GBufferMaterial.Load(int3(pixel,0)).rgb * 0.5f + 0.1f` and routes through if-alive/else-blue/default-gray. The discriminator design is sound.
- **Mode-30 source verification**: Confirmed case 30u at `GIPathTracing.hlsl:764-773` (per v180 plan critique line 31). The single-pixel sentinel at (0,0,0) writes magenta iff SRV returns >0; else black.
- **diff_estimate**: +174/-0 lines is plausible for the listed 8 sub-steps (gate 8 alone is ~60 lines of inline Python + per-leaf echo + case-esac; gate 9 is similar). Plan is realistic.

## Areas where the plan could be sharper (non-blocking)

1. The plan's "Empirical refutation risk" (line 64-66) acknowledges the DIAGNOSTIC_2026-07-30 → DIAGNOSTIC_2026-08-19-REFUTED tension but does not state an explicit decision: the v180 cycle's discriminator design is preserved regardless of whether the test is currently passing. The honest scope caveat (lines 86-92) does clarify this — "discriminator modes (gates 8/9) are skipped by default and only run if the operator passes `--mode-30` or `--mode-31`." Approved.

2. The plan's carry-forward note (lines 78-83) lists "Operator-side action required: nothing — the patch is already on disk" — this is the REVERSE of the v180 cycle's expectation (which required operator-side `git apply`). The reversal is correct for v181 because the cron has `patch` tool access and can apply the recipe changes directly. Approved.

3. The plan's risk #4 correctly identifies that the v181 patch does not pivot to the REFUTED diagnostic, AND that the discriminator design is orthogonal to whether the test is currently passing. This is the conservative and correct read. Approved.

## Feedback for planner (FIX only)

(none — the plan is sound and is the file-only execution of the v180 staged patch. The 3 non-blocking sharpening points above are folded into the impl cycle, not requiring a re-plan.)

## Verdict rationale

KEEP because:
- The patch scope is the exact 3 RELAX items from v180 audit (BLUE-MID + gray-mid envelopes + --mode-30 flag)
- The patch is purely additive (no existing lines changed) — safe to apply
- All deliverables are file-only-feasible (recipe patch via `patch` tool, marker files via `write_file`)
- The discriminator design from v180 is preserved — if the operator passes --mode-31 or --mode-30, the recipe detects the leaf and routes to the discriminator verdict
- The empirical refutation risk is acknowledged and handled (discriminator is orthogonal to current-pass state)
- The skip_plan_review and skip_impl_review markers are correctly used; cycle cost is 3 ticks (plan + impl + test-audit)

Route to impler (Rule 4 from state machine — KEEP verdict, no commit → impl).

## Note for parent session

This cycle closes the v180 SOME_RELAX wrap-up. v180 remains SOME_RELAX in the audit; v181 is its closure cycle, not a re-litigation. The empirical refutation in `DIAGNOSTIC_2026-08-19-gpuTex-zero-REFUTED.md` stands as before — operating on the 2026-07-30 authoritative diagnostic is the v182 anti-pattern per `six-role-pipeline §Anti-patterns §6`.

The next actionable step for the operator (if any) is: run `bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh --skip-build` (post-patch) to confirm exit 0 on the freshest-available binary + dump. Per `DIAGNOSTIC_2026-08-19-gpuTex-zero-REFUTED.md` evidence, this should exit 0 (8 gates closed, 4-check validator PASS, display stats `mean=[0.4584,0.4581,0.4861] std=[0.0458,0.0470,0.0429]` = recognizable Sponza). The discriminator gates 8/9 are optional and skipped by default — the operator only runs them if a regression recurs.
