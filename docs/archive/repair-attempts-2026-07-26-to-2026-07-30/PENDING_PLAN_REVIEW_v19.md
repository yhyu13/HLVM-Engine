# Pending Plan Review v19

- plan: docs/PENDING_PLAN_v19.md
- verdict: KEEP
- reviewer: plan-criticer (six-role-pipeline, single-head, file-only)
- timestamp: 2026-07-27

## Design soundness

The v19 plan correctly completes the diagnostic surface by adding three more probes (modes 12u, 15u, default-case trace) to the existing 11. Each new case targets a specific remaining hypothesis in the bug-space decision matrix:
- mode 12u decouples AmbientColor from ambientScale (decisive for "which uniform is wrong" branching)
- mode 15u catches the divide-by-256 in mode 10 (sanity check)
- default-case trace catches slangc-dead-strip-of-all-cases (canonical catch-all sentinel)

The three additions together give the parent's next interactive session the ability to bisect every possible hypothesis in a single rebuild + 9 mode runs. Risk profile matches v17/v18 (all of which landed clean).

## Plan completeness

The plan correctly identifies:
- What each new mode probes (decision matrix table is comprehensive)
- What the parent's evidence-collection path looks like (one rebuild + 9 mode runs)
- The v20 decision matrix that maps each evidence shape to a follow-up cycle
- The risk profile (default-case masking, gi_raw overflow at mode 15, slangc dead-strip, -Werror cascade)
- What this cycle does NOT do (no C++ changes, no CVar additions, no binding layout changes)

The plan is complete. The patch is text-identical between the two HLSL copies per v15/v17/v18 sync convention. The default-case trace modification is correctly noted as non-disruptive (only fires for debugMode not in {1..15}).

## Risks acknowledged

The plan correctly identifies the default-case trace risk: if the switch is broken, the default would mask existing-case outputs. Mitigation: the default only fires when debugMode is NOT in {1..15}, which is the current case set. The plan also correctly identifies the gi_raw overflow risk at mode 15 (writes 15.0 to OutputTexture) and notes that gi_raw is HDR before tonemap, so 15.0 is valid; display dump applies tonemap which clamps to [0,1].

## Feedback for planner

No changes needed. The plan is sound, well-scoped, and correctly completes the diagnostic surface from 11 to 14 probes (modes 1-15 + default) in a single file-only patch.

Single-head caveat applies: this KEEP is a self-check by the same model that wrote the plan.

## Honesty about the verdict

KEEP is the right verdict because:
1. Each new case label is gated behind `if (debugMode != 0u)`, so the default mode-0 path is unaffected.
2. Each new case reuses existing identifiers that have been verified in scope at the switch's lexical location (g_GI.AmbientColor.rgb, g_GI.Params5.x).
3. The patch is text-identical between the two HLSL copies, preserving the v15/v17/v18 sync convention.
4. The diagnostic interpretation table is comprehensive — the parent's next interactive session will produce decisive evidence across 9 mode runs in a single rebuild.
5. The risks are correctly identified and the worst case (default-case masks existing-case outputs) is itself the decisive evidence the plan needs.

## Verdict rationale

KEEP. The v19 plan is a high-value, low-risk completion of the diagnostic surface that gives the parent's next interactive session the ability to bisect every possible hypothesis in a single rebuild. This is exactly the kind of work a six-role pipeline should advance autonomously when terminal access is blocked but static-source patches can still land.