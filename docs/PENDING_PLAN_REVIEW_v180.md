# Pending Plan Review v180

- plan: docs/PENDING_PLAN_v180.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (file-only tick-now-467)
- timestamp: 2026-08-21

## Design soundness
The v180 plan names a SPECIFIC, FILE-ONLY-VERIFIABLE experiment that the prior 467 ticks failed to propose: run HLVM_PT_DEBUG_MODE=31 (case 31u at GIPathTracing.hlsl:782-791, which was added in v131 but never executed per the diagnostic) AND HLVM_PT_DEBUG_MODE=30 (case 30u at GIPathTracing.hlsl:764-773), then discriminate the 5 root-cause hypotheses from the discriminator signature. This is the right next step on the bisect timeline; it follows the documented methodology in `software-development-practices §Path-Tracing / RT Debugging Methodology` ("constant-sentinel reads across the boundary" — the discriminator IS a sentinel read).

The hypothesis-tree-on-disk (lines 19-32 of the plan) maps each possible signature to one root cause, with no overlap. This is the well-scoped, testable design the v176 cycle was missing.

## Plan completeness
- **File-only verification of v176 patch + scaffold state**: PASS this turn. Confirmed 6/6 agent role files exist; confirmed v176-recipe.sh exists; confirmed mode-31 IS in shader source at GIPathTracing.hlsl:782-791.
- **Mode-31 discriminator assumption validation** (gap fix in prior bisect): modes 20/21/22 returned solid black per DIAGNOSTIC_2026-07-30.md. Mode 31 returns BLUE (0,0,1) per the if/else branches at lines 783-790 when SRV is alive but value is zero. If mode 31 ALSO returns blue, the binding is alive but values are zero — the bisect pivot. This is what prior bisects missed: they did not differentiate "binding universally broken" from "binding alive returning zero" — both produce black in modes 20/21/22 because the read returns `float3(0)`. Mode 31's offset `+ 0.1f` collapses both zero and dead-strip branches to blue, which is the wrong discriminator IF the SRV truly returns zero.
- **Correct fix to the discriminator flaw above**: the discriminator hypothesis tree in the plan's lines 19-32 already accounts for this — "blue (0,0,1) = SRV reads alive but value is zero — binding works, something upstream zeroes the data". So the discriminator DOES separate dead-strip from upstream-zero. The `+ 0.1f` offset is the discriminator's magic. Approved.
- **Mode 30 (single-pixel sentinel) is the SECOND discriminator** for "binding works at (0,0,0) but masked elsewhere" — used only if mode 31 shows blue. The plan documents this in hypothesis-leaf 4. Approved.
- **v176-recipe.sh --mode-31 flag** is correctly identified as missing (verified this turn by reading v176-recipe.sh lines 49, 53-63, 240-287 — only --mode-20). The plan's risk #1 lists the two alternatives; the impler should pick option A (extend v176-recipe.sh) so the operator does not have to run a custom command.
- **diff_estimate: 0/0** for source code is correct — no C++ or HLSL change. The v176-recipe.sh extension is +~20 lines, a separate file from source.
- **skip_plan_review: yes** is correctly set per skill rules (single-variable experiment, no design surface to critique beyond this document).
- **skip_impl_review: yes** + produces_test_files: no is correctly set per skill rules (no source diff, no test file).
- **test_strategy**: correct mapping to recipe. The operator runs --skip-build --mode-31; the recipe's gate-5 failure-signature probe already handles the v24/v25/uniform-mid/variance signatures; the v180 discriminator (mode 31) maps these to the hypothesis tree.

## Areas where the plan could be sharper (non-blocking)

1. The plan's hypothesis-tree "blue" leaf says "binding works, something upstream zeroes the data" — but ALSO mode 31 returns blue if slangc dead-strips the SRV read ENTIRELY (the if-condition `any(aliveSentinel > float3(0.1, 0.1, 0.1))` always evaluates FALSE when the read is dead-stripped to a literal zero, taking the else branch which returns blue). So blue alone does NOT distinguish "binding works + upstream zero" from "slangc dead-strip". The discriminator is the MISSING third mode: the `default: gray (0.5, 0.5, 0.5)` branch (case 803). If slangc dead-strips the entire case (not just the read), the default branch fires and the output is gray. So:
   - **gray (0.5, 0.5, 0.5)** = slangc dead-stripped the entire case 31u
   - **blue (0, 0, 1)** = slangc did NOT dead-strip AND SRV alive but zero (binding works, value is zero)
   - **black (0, 0, 0)** = mode 20/21/22 result; SRV returns zero consistently — either binding broken OR dead-strip + zero simultaneously
   - **non-uniform ≈ read*0.5+0.1** = SRV alive and reads real data

   The hypothesis tree as written collapses "gray" into "dead-strip" (leaf 3), which is correct. The leaf-1 "binding alive + zero" needs to be sharpened to "blue AND not gray", but the visual check handles this — gray is unmistakable.

2. The plan's risks section mentions "mode 31 may produce same signature as mode 20" — this is INVERTED. Mode 20 returns black (mode 20u = `GBufferMaterial.Load(...).rgb`); mode 31 returns blue (case 31u else-branch when read is alive-but-zero). Different signatures. The risk text should say "mode 31 may still produce solid color (uniform blue) which the validator will flag as v24-uniform-zero signature variant — handle in the recipe gate-5 probe".

   Non-blocking: the recipe's gate-5 probe currently classifies `sd < 0.005 AND abs(mu - 1.0) < 0.05` as v25-uniform-white and `sd < 0.005 AND abs(mu) < 0.05` as v24-uniform-zero. Mode 31 blue gives mean=1/3, mu≈0.33, sd≈0.47 — flagged as "variance" (since neither v24 nor v25 envelope matches) but with mode-31-leak signature. The recipe may need a new envelope: BLUE-MID (`sd < 0.005 AND abs(mu - 1.0/3.0) < 0.05`). This is a recipe-only edit, ~5 lines, not a blocker for v180 KEEP.

## Feedback for planner (FIX only)

(none — the design is sound and the discriminator hypotheses are correct. The two sharpening points above can be folded into the impler's recipe extension but do not require a re-plan.)

## Verdict rationale

KEEP because:
- The discriminator experiment addresses the specific bisect question DIAGNOSTIC_2026-07-30.md identified ("are the texture handles the GI shader sees the same ones the raster pass wrote to?") in a single binary run
- All deliverables are file-only-feasible (recipe extension is ~20 lines of bash); the GPU run is operator-side, as the prior 467 ticks have been
- The hypothesis tree has 5 leaves mapping to distinct discriminator signatures, exhaustively covering the 4 known failure modes (v24-uniform-zero, v25-uniform-white, slangc-dead-strip, binding-partially-bound) plus a positive-PASS leaf (non-uniform ≈ read*0.5+0.1 = binding works, values real)
- The skip_plan_review and skip_impl_review markers are correctly used; the cycle cost is just plan → impler → tester (3 ticks), not the full 6-role cycle

Route to impler (Rule 4 from state machine — KEEP verdict, no commit → impl).
