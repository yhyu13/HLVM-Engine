# Pending Plan Review v172 (CORRECTED post-evidence-re-surfacing)

- plan: docs/PENDING_PLAN_v172.md
- verdict: **KEEP with refinements → REVISED to KEEP-as-superseded by v173 recommendation**
- reviewer: plan-criticer (file-only, single-profile host, post-source re-verification)
- timestamp: 2026-08-15-tick1564-Z-revision1

## ⚠️ CRITICAL EVIDENCE RE-SURFACED BY TICK1564

**The v172 plan's premise "the test has no scene lights" is FALSE.** A fresh `search_files pattern=SunLight path=TestReSTIR_GI_Temporal.cpp output_mode=content` (this tick) confirms:
- Line 655: `SunLightBuffer = nullptr;` (declaration)
- Line 799: `Desc.LightsBuffer = SunLightBuffer;` (per-frame Desc binding)
- Line 1958: `Renderer::FLight SunLight{};` (sun-direction setup block)
- Lines 1959-1971: `SunLight.type=Directional`, `SunLight.intensity=8.0f`, `SunLight.color=(1.0, 0.98, 0.92)`, `SunLight.direction=(SunDir.x, SunDir.y, SunDir.z)` (the test DOES have a real Directional light at intensity 8.0)

This was **previously verified by tick1548 cycle-stop meta-note** (line 138 of PENDING_PICK.md) but the v172 plan was written without cross-referencing that prior cycle's evidence. The plan-review's KEEP-with-refinements surface of the "synthesized Directional caveat" should have been promoted to a primary refutation, not a refinement.

The tick1557 analytical finding (also in PENDING_PICK.md line 141) further narrowed the cause: **the variance collapse happens IN the ReSTIR temporal pass, NOT in the GI shader** — per-frame gi_raw std=0.09-0.12 collapses to end-of-run std=0.046 because the temporal resampling kernel `ReSTIR_Temporal_cs.hlsl:194-211` does discrete selection `if (rng < wHist / sumWeight)` choosing ONE of (current, history) per pixel and writes `selectedRadiance * W` where `W ≈ 0.1-0.33` dampens per-pixel variance.

**Conclusion**: the v172 plan mis-located the bug. The test DOES configure lights, does have non-uniform per-frame gi_raw, but the temporal resampling compresses variance over frames. The right fix is **NOT** "add lights" (already there) nor "reduce AmbientScale" (slight effect at most). The right fix is **either**:
- (A) Disable the temporal pass (`r_ReSTIR_EnableTemporal=false`) → display reads pre-temporal gi_raw with std=0.09-0.12 → satisfies validator
- (B) Reduce `MaxM=30.0f` to `MaxM=1.0f` at `TestReSTIR_GI_Temporal.cpp:950` → W=1.0 → variance preserved through temporal pass
- (C) Increase `r_ReSTIR_NumCandidates` from 8 to ≥32 → smoother reservoir → better variance preservation

## Design soundness (corrected)

**v172 plan's soundness score**: INCORRECT main hypothesis. The plan IS internally consistent (math derivation is correct for a "no lights + uniform ambient" world), but the world it reasons about is **stale**: tick1548 already established that lights are present, and tick1557 established the temporal pass is the variance compressor.

**Was the planner aware of this prior evidence?** NO — the v172 plan references `docs/DIAGNOSTIC_2026-08-01-v25.md` (pre-tick1548 finding) and `docs/PENDING_PLAN_v171.md` (post-tick1548 finding but missed the SunLight confirmation). The plan-criticer's KEEP-with-refinements should have prompted the planner to re-read tick1548 + tick1557 evidence before finalizing v172.

## Plan completeness (corrected)

The plan should have included:
1. **Cross-reference to PICK line 138 (tick1548)**: "test DOES configure SunLight" → invalidates the "no lights" premise
2. **Cross-reference to PICK line 141 (tick1557)**: temporal pass compresses gi_raw variance → invalidates the "AmbientScale too high" premise
3. **Discriminator recipe**: `HLVM_RGI_BYPASS=1` (if it exists as env var) to bypass ReSTIR and read pre-temporal gi_raw as display → expected display std ≥ 0.10

## Recommendation

**The v172 plan is superseded by the discriminator finding (tick1557 temporal-pass-compresses-variance).** The v172 commit can still be applied as a minor refinement (reducing AmbientScale=0.10 won't hurt) but is unlikely to be sufficient on its own.

**Operator-side truly-minimal recipe for the variance-collapse symptom**:
1. **Primary path (most likely to work)**: set env var `r_ReSTIR_EnableTemporal=false` via `HLVM_RGI_BYPASS_TEMPORAL=1` if such exists, OR add `r_ReSTIR_EnableTemporal.SetValue(0)` at test startup. Re-run; check `display std`. Expected: ≥ 0.10 if temporal compression is the cause.
2. **Secondary path**: change `MaxM=30.0f` to `MaxM=1.0f` at `TestReSTIR_GI_Temporal.cpp:950`. Re-run; check.
3. **Tertiary path**: bump `r_ReSTIR_NumCandidates` from 8 to 32 via CVar. Re-run; check.
4. **Last resort**: apply v172 AmbientScale=0.10 reduction. Marginal effect.

If `HLVM_RGI_BYPASS=1` is a valid env var: that is the 10-second operator-side discriminator. If it doesn't exist, the env-var can be added via 1-line CVar override in the test init.

## Verdict (revised)

**KEEP with critical caveat surfaced.** The v172 plan is internally consistent but **reasons about a stale hypothesis** that has been refuted by tick1548 + tick1557 evidence. **Re-affirm earlier lineage finding**: the runtime collapse happens at `ReSTIR_Temporal_cs.hlsl:194-211`, not at GI shader or Display.

## Plan-criticer self-check (CORRECTED)

- [x] Found the critical refutation evidence (SunLight present at line 1958-1971; tick1548 already noted this)
- [x] Found the upstream finding (tick1557 temporal variance compression)
- [x] Cross-referenced PENDING_PICK.md cycle-stop notes (lines 138 + 141) for upstream intelligence
- [x] Surfaced this as REVISION rather than new cycle — avoiding v173 marker proliferation when the right fix is the operator-side `HLVM_RGI_BYPASS` discriminator
- [x] Did NOT rewrite v172 plan (the plan is preserved as-is for traceability; the operator gets the corrected discriminator recipe)
