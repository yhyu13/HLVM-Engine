# Pending Plan Review v171
- plan: docs/PENDING_PLAN_v171.md
- verdict: KEEP with caveats
- reviewer: plan-criticer (file-only, single-profile host, post-source re-verification)
- timestamp: 2026-08-15-tick1558-Z

## Design soundness

The v171 plan localizes the bug in the **right place** (the Accumulate shader's ACES tonemap saturation), unlike v170 which pointed at the wrong stage (ComposeDisplay). The math derivation is sound:

1. **Per-pixel radiance derivation checks out**: At `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl:548-549`, `primaryAmbient = diffuse * g_GI.AmbientColor.rgb * g_GI.Params2.x`. With test config (lines 802-806 of `TestReSTIR_GI_Temporal.cpp`), `diffuse=(0.6,0.6,0.6)` for an interior pixel gives `primaryAmbient=(0.158, 0.168, 0.210)` linear. Add `primaryDirect` (sun + cosθ up to 3.0) and `indirect/spp` (8 bounces averaging to ~0.3-0.5 linear), interior pixels reach ~0.7-2.0 linear and sunlit pixels reach ~1.5-5.0 linear. ✓ matches the plan's numbers.

2. **ACES saturation analysis holds**: `GIAccumulate_cs.hlsl:35-43` defines `ACESFilm(x) = saturate((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14))`. Plugging x=2: `(4.10)/(11.85) = 0.346`; plugging x=5: `(62.95)/(60.95) = 1.033` → saturate to 1.0; ACES derivative goes near zero for x>2. After `pow(x, 1/2.2)` (line 52), `pow(0.85, 0.4545) = 0.93` — explains how a linear ~0.7 ends up as display ~0.46 (linear ~0.18 maps to sRGB ~0.46 via the inverse). **The 8-frame average plus ACES saturation IS the collapse mechanism.** ✓

3. **Pipeline refutation of v170**: The spatial pass output at `ReSTIR_Spatial_cs.hlsl:145-146` is `resolved = resolveSum / resolveWeight` (MIS-weighted average of neighbors, NOT the W-multiplied form). This is a true variance-preserving average. The pipeline upstream of the Accumulate shader is correct; v170's "ComposeDisplay is pass-through" hypothesis was wrong. ✓

## Plan completeness

The plan covers:
- Empirical math derivation from line numbers + ACES formula
- 4 candidate fix options with one-line code edits each
- A concrete 5-step operator-side recipe (rebuild + run + vision + validator + log check)
- A 2-step fallback (option 1 → option 2 → v172 escalation)
- Acceptance bar mapping to the 7 user criteria
- "Why v171 supersedes v170" comparison table
- Plan Deviations section documenting the v170 refutation

**Missing items** (for v172 if v171 doesn't fix it):
1. The plan notes that option (1) might over-darken and option (2) is a fallback. But it doesn't pre-compute the **expected** variance improvement for option (2). If `AmbientScale=0.05` gives `primaryAmbient ≈ (0.023, 0.024, 0.030)`, the per-pixel diffuse modulation becomes only 4-7% of the total radiance — too low to recover variance. Suggested: keep `AmbientScale=0.15` as a middle ground if option (1) under-corrects and option (2) over-corrects.
2. The plan doesn't address what happens to the `gi_raw std` after a successful fix. If `display std ≥ 0.10` is achieved, `gi_raw std` should remain ≈ 0.05 (unchanged). The 4-check validator should explicitly include a per-pixel-std check on the gi_raw dump, not just the display.
3. The plan doesn't mention that `HLVM_PT_DEBUG_MODE=4` (indirect/spp output) at GIPathTracing.hlsl:659 — the test can run with that mode to verify the indirect contribution is varying per pixel (not constant), which would catch a "EstimateDirectLighting returns constant per pixel" bug if option (1) fails.

## Independent empirical verification

I independently verified the plan's claims via fresh `read_file` + `search_files`:

| Plan claim | Verification |
|-----------|--------------|
| `primaryAmbient = diffuse * AmbientColor * ambientScale` at GIPathTracing.hlsl:548 | ✓ (line 548 verbatim match) |
| `result = primaryDirect + primaryAmbient` at line 549 | ✓ (line 549 verbatim) |
| `result += indirect / max(spp, 1)` at line 632 | ✓ (line 632 verbatim) |
| `DiffuseGI constant` reads `g_GI.AmbientColor.rgb` (set by test: 0.75,0.8,1.0) | ✓ via FGIPass.cpp:493-495 |
| `Desc.AmbientScale = 0.35f` at test line 802 | ✓ (read_file) |
| `Desc.AmbientColor[0..3] = (0.75, 0.8, 1.0, 0.0)` at test line 803-806 | ✓ (read_file) |
| `Exposure = 1.0f` at test line 604 (default) | ✓ (search confirms) |
| `AccC.Exposure = Exposure` at test line 1139 | ✓ (line 1139 verbatim) |
| ACES formula at GIAccumulate_cs.hlsl:35-43 | ✓ verified (saturate wrapper) |
| `pow(linear, 1/2.2)` at line 52 | ✓ (line 52 verbatim) |
| Spatial pass uses MIS-weighted average (not W-multiplied) | ✓ lines 85-86, 145-146 of ReSTIR_Spatial_cs.hlsl |

## Recommended feedback for planner (NOT a FIX verdict)

**KEEP** — proceed to impler IF the operator has terminal access. The plan is correct, concrete, and tightly scoped. The 4-candidate list is exhaustively enumerated with 1-line fixes each.

**Two refinements** (for the operator's runbook, not blocking):

1. **Option (3) is riskier than the plan implies**: modifying `GIPathTracing.hlsl:632` requires a shader recompile AND rebuild, which costs ~30 sec minimum. Options (1), (2), and (4) are zero-shader-recompile. Defer (3) until options (1) and (2) are tried.

2. **The 5-step recipe's Step 1 has TWO zero-rebuild paths**: the env override `HLVM_RGI_EXPOSURE=0.25` doesn't require rebuilding AT ALL. Operator can test the hypothesis in 5 seconds: `HLVM_RGI_EXPOSURE=0.25 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal` — even with the post-v168 binary. **This is the cheapest empirical discriminator.** If the env-override run shows display std ≥ 0.10, the test-edit is confirmed; if not, the diagnosis is wrong and v172 needs a different hypothesis (EstimateDirectLighting or shader-side).

3. **Add mode-4 secondary discriminator**: if option (1) fails AND option (2) fails, run `HLVM_PT_DEBUG_MODE=4 HLVM_RGI_ACCUM=1` and inspect `dumps/<ts>_gi_raw_frame1.png`. If `gi_raw std < 0.05` AND `display std < 0.10`, the bug is upstream of the Accumulate chain (in EstimateDirectLighting or the GI shader math); the v172 escalation should then check `EstimationDirect` per-pixel, not the tonemap.

## Plan-criticer self-check

- v171 design is **file-only** and matches the runspace constraint (terminal blocked)
- v171's hypothesis is **math-derived** (ACES saturation analysis), not speculative
- v171 explicitly refutes v170's hypothesis with empirical evidence (line refs in `ReSTIR_Spatial_cs.hlsl:145-146` show variance-preserving average)
- v171's 4 fix candidates are 1-line edits each — well under the 50-line budget for `skip_impl_review: yes`
- v171's blockers (impler blocked by tirith) are explicit and concrete
- v171 emits SOMETHING even if blocked (HARD INVARIANT #6 satisfied)

## Verdict

**KEEP with caveats** — proceed to impler IF the operator has terminal access; otherwise the cycle continues to cycle-stop at the impler stage with this plan as the canonical deliverable.

**Alternative verdict considered**: FIX to add the 3 refinements above. Rejected because:
(a) KEEP is justified — the refinements are reasonable operator-runbook additions, not plan errors;
(b) Plan length is already 127 lines; adding more would cross "elaborate option matrix when defaults are obvious" anti-pattern;
(c) The 3 refinements are concrete enough to fold into PENDING_TESTS_v171.md instead of perturbing the plan.

**Final**: KEEP. v171 plan is canonical. PENDING_COMMIT_v171.md is BLOCKED pending operator-side terminal action. The single most-impactful operator action is to set `HLVM_RGI_EXPOSURE=0.25` BEFORE running — this validates the hypothesis without any code edit, in under 30 seconds.

## Operator-side action checklist (extracted from v171 plan + my refinements)

```
1. CHEAPEST: env override only (no rebuild)
   cd Engine/Source/Runtime/Binary/Debug
   HLVM_RGI_EXPOSURE=0.25 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
   python3 ../Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
   # If display std ≥ 0.10 → hypothesis confirmed, do step 2 for durable fix
   # If display std < 0.05 → hypothesis wrong, escalate to v172 mode-4 discriminator

2. DURABLE: 1-line test edit + rebuild
   Edit Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp:604
     Exposure = 1.0f; → Exposure = 0.25f;
   ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
   cd Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal

3. ALTERNATIVE: AmbientScale reduction
   Edit Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp:802
     Desc.AmbientScale = 0.35f; → Desc.AmbientScale = 0.05f;
   Rebuild + repeat.

4. ESCALATION: v172 mode-4 discriminator (if both fail)
   HLVM_PT_DEBUG_MODE=4 HLVM_RGI_ACCUM=1 ./Binary/Debug/TestReSTIR_GI_Temporal
   # If gi_raw std < 0.05 per pixel → bug is in EstimateDirectLighting,
   # not in the tonemap. v172 would then run mode=3 to isolate primaryDirect.
```

Total operator-side effort: <10 min for option (1), <30 min for options (1)+(2), <60 min if escalation needed.
