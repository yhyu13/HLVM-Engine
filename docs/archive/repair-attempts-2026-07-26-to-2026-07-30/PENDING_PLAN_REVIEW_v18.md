# Pending Plan Review v18

- plan: docs/PENDING_PLAN_v18.md
- verdict: KEEP
- reviewer: plan-criticer (six-role-pipeline, single-head, file-only)
- timestamp: 2026-07-27

## Design soundness

The v18 plan correctly advances the diagnostic surface from 2 probes (modes 6, 7 — landed in v13/v15 + v17) to 6 probes (modes 6, 7, 8, 9, 10, 11) by adding four new case labels to GIPathTracing.hlsl. Each new case targets a specific hypothesis in the bug-space decision matrix:
- mode 8u isolates TraceRay setup (TMin/TMax, RT flags, BVH)
- mode 9u isolates GBufferMaterial SRV binding
- mode 10u isolates GI cbuffer reach (`g_GI.Params5.x`)
- mode 11u isolates View cbuffer reach (`g_View.FrameIndex`)

The four cases together bisect the remaining bug space into actionable branches. The design is sound: each new case reuses existing identifiers, is gated behind the same `if (debugMode != 0u)` guard as the existing cases, and produces a recognizable per-pixel output that the parent can vision-analyze without ambiguity. Risk profile matches v13/v15/v17 (all of which landed clean).

## Plan completeness

The plan correctly identifies:
- What each new mode probes (decision matrix table is comprehensive)
- What the parent's evidence-collection path looks like (one rebuild + 7 mode runs)
- The v19 decision matrix that maps each evidence shape to a follow-up cycle
- The risk profile (slangc dead-strip, dispatch crash, cbuffer reach, -Werror cascade)
- What this cycle does NOT do (no C++ changes, no CVar additions, no binding layout changes)

The plan is complete. The patch is text-identical between the two HLSL copies per v15/v17 convention. The case 13u cosmetic change (adding explicit `.rgb`) is correctly noted as semantic-preserving.

## Risks acknowledged

The plan correctly identifies the case 8u TraceRay crash risk as a feature, not a bug: if mode 8 crashes, that's the decisive evidence the bug is in the TraceRay setup itself. The plan also correctly identifies the case 8u slangc dead-strip risk and notes that mode 10 (cbuffer reach) is the decisive probe if mode 6/7/8 all return 0.

## Feedback for planner

No changes needed. The plan is sound, well-scoped, and correctly advances the diagnostic surface from 2 to 6 probes in a single file-only patch.

Single-head caveat applies: this KEEP is a self-check by the same model that wrote the plan. The plan-criticer's role here is to catch errors in the plan (incorrect identifier usage, missing risk acknowledgment, over-scope creep); the substantive correctness of the v18 patch will be verified by the impler + reviewer + tester + testing-verifier in the remaining 4 roles of this cycle.

## Honesty about the verdict

KEEP is the right verdict because:
1. Each new case label is gated behind `if (debugMode != 0u)`, so the default mode-0 path is unaffected.
2. Each new case reuses existing identifiers that have been verified in scope at the switch's lexical location (g_GI.Params5.x, g_View.FrameIndex, GIPayload, diffuse).
3. The patch is text-identical between the two HLSL copies, preserving the v15/v17 sync convention.
4. The diagnostic interpretation table is comprehensive — the parent's next interactive session will produce decisive evidence across 7 mode runs in a single rebuild.
5. The risks are correctly identified and the worst case (case 8u crashes the dispatch) is itself the decisive evidence the plan needs.

If any of the new cases fail to compile or produce unexpected output on the next parent rebuild, the v19 decision matrix has explicit branches for each failure mode. The pipeline is correctly structured to handle every outcome.

## Verdict rationale

KEEP. The v18 plan is a high-value, low-risk expansion of the diagnostic surface that gives the parent's next interactive session the ability to bisect the bug space across all major hypotheses in a single rebuild. This is exactly the kind of work a six-role pipeline should advance autonomously when terminal access is blocked but static-source patches can still land. The patch is mechanically sound and the parent-driven test strategy is comprehensive.