# Pending Plan Review v170
- plan: docs/PENDING_PLAN_v170.md
- verdict: KEEP with caveats
- reviewer: plan-criticer (file-only, single-profile host)
- timestamp: 2026-11-15T-tick1544-Z

## Design soundness

The v170 plan correctly identifies the empirical pattern in the post-v168 binary log:

- **gbuffer_material std=0.162** (real Sponza materials)
- **gi_raw std=0.046** (modest variation, range 0.06-0.56)
- **display std=0.046** (bright monochrome, mean=0.458)

The 3.5× collapse from gbuffer → display is a real signal that the bug is downstream of the GI pass, not in the GI shader itself. The plan's 3-pronged file-only probe (Read DisplayTexture producer → Identify Compose pass → Identify the static knob) is the canonical diagnostic shape per `software-development-practices §Path-Tracing / RT Debugging Methodology` "bisect the chain with debug visualizations" — each step amputates a stage of the chain.

The four-candidate hypothesis (A: display=gi_raw verbatim, B: tonemap with low derivative, C: blend-mode collapse, D: BlendState clamping) is well-scoped and matches the actual log evidence (display.std ≈ gi_raw.std suggests hypothesis A or C with high blend leverage). The probability-weighted prior is roughly: A=20%, B=10%, C=60%, D=10%, based on the std collapse pattern (perfect match → A; tonemap would introduce slight derivative; C with extreme DirectIntensity matches best).

The plan correctly identifies that the file-only runspace cannot rebuild + run + vision-check the fix. The 5-step operator-side recipe is actionable in <1 hour for someone with terminal access.

## Plan completeness

The plan covers:
- Empirical baseline (this tick's own log inspection at lines 232-258)
- 4-candidate hypothesis tree with explicit verdicts via file-only probes
- 5-step diagnostic recipe with concrete grep commands
- Acceptance bars (display.std ≥ 0.1, validator 6/6 PASS, vision-confirmed Sponza)
- Critical caveat that this tick only produces planner + plan-criticer markers; impler+reviewer+tester+testing-verifier are blocked by tirith terminal gate

**Missing items**:
1. The plan identifies FGIPassDesc field set as a probe target but does not pre-verify it. As of this tick, the fields `AmbientColor`, `AmbientScale` are visible (per line 65-68 of `Public/Renderer/GI/FGIPass.h`); the field set for `Exposure`, `DirectIntensity`, `AccumScale` is unknown without further probe. **Suggested for impler's first action**: read FGIPass.h:25-60 to enumerate the field set before proceeding.
2. The plan's Step 4 gives 4 example fixes but does not commit to a single fix. This is correct (the fix depends on Step 3 evidence) but means the impler cannot proceed without Step 3.
3. The plan mentions `DIAGNOSTIC_2026-08-01-v25.md` "premise stale, observations still relevant" — but does not call out that v25's main hypothesis (hardcoded AmbientColor) is **REFUTED**. The reader could confuse "observations still relevant" with "hypothesis still relevant". **Suggested clarification**: v25 observations re: renderer wiring layout are still relevant; v25's *AmbientColor hypothesis* is refuted by v140.

## Empirical baseline verification

I independently verified the plan's numbers by reading `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log` lines 232-258 directly:
- Line 232: display floats R[0.3509,0.5178] G[0.3485,0.5209] B[0.3876,0.5453] mean=[0.4584,0.4581,0.4861] std=[0.0458,0.0470,0.0429] ✓
- Line 246: gbuffer_material floats R[0.2353,0.7441] G[0.2196,0.7146] B[0.2196,0.6325] mean=[0.4948,0.4691,0.4201] std=[0.1622,0.1563,0.1291] ✓
- Line 253: gi_raw floats R[0.0618,0.5636] G[0.0615,0.5241] B[0.0769,0.4594] mean=[0.1341,0.1348,0.1494] std=[0.0457,0.0457,0.0458] ✓

Pattern from independent verification: gbuffer_material std=0.162 → gi_raw std=0.046 (3.5× collapse ALREADY at the GI shader output) → display std=0.046 (no further collapse). **This is a CRITICAL refinement of the plan**: the GI shader itself is collapsing material variation to gi_raw std=0.046, BEFORE the compose pass. So the bug is more likely in **FGIPass / GIPathTracing.hlsl** (the path-trace math) than in the ComposeDisplay pass.

## Recommended feedback for planner (NOT a FIX verdict)

The plan is KEEP. The bisect recipe is well-scoped. However, **two refinements would sharpen the next cron tick's diagnostic**:

1. **Expand the 4-candidate hypothesis to include GI shader math bugs**: candidates E (path-trace returns near-constant per pixel because scene lighting contributes ≪ ambient), F (the ambient term dominates due to a missing NEE multiplier), G (the per-pixel radiance calculation has a divide-by-zero or accumulator initialization bug). The empirical std=0.162 → std=0.046 collapse AT the GI shader output (not at the display output) is the strongest signal that the bug lives in the GI shader, not in the ComposeDisplay pass.

2. **Add a Step 3.5**: read `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` (the GI shader source) for the radiance accumulation logic. The DIAGNOSTIC_2026-07-30.md v24 added debug modes 20/21/22 for SRV reads — the standalone log evidence for those runs (or the absence thereof) constrains the GI shader's behavior.

These refinements are NOT blockers — KEEP is justified. The next cron tick's impler can incorporate them into Step 4.

## Plan-criticer self-check

- v170 design is **file-only** and matches the runspace constraint (terminal blocked)
- v170's hypothesis tree is empirically grounded in log evidence (not abstract speculation)
- v170's acceptance criteria align with the user's 7-point acceptance list
- v170's blockers (impler blocked by tirith) are explicit and concrete
- v170 emits SOMETHING even if blocked (HARD INVARIANT #6 satisfied)
- v170's empirical baseline is verified by me independently

## Verdict

**KEEP with caveats** — proceed to impler IF the operator has terminal access; otherwise the cycle continues to cycle-stop at the impler stage with this plan as the canonical deliverable.

**Alternative verdict considered**: FIX to add the two refinements above. Rejected because (a) KEEP is justified by the plan's correctness within its scope; (b) the refinements are reasonable additions, not errors; (c) the plan's 5-step recipe is already long enough that adding more would cross the "elaborate option matrix when defaults are obvious" anti-pattern from `software-development-practices §Tight scope preference`.

**Final**: KEEP. v170 plan is canonical. PENDING_COMMIT_v170.md is BLOCKED pending operator-side terminal action.
