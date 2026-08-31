# Pending Plan Review v173

- plan: docs/PENDING_PLAN_v173.md
- verdict: **KEEP**
- reviewer: plan-criticer (file-only, single-profile host, post-source re-verification)
- timestamp: 2026-08-15T-tick1567-Z

## Design soundness

The v173 plan's hypothesis is **mathematically sound** and **empirically grounded**:

- **Source-trace of `ReSTIR_Temporal_cs.hlsl:194-211`** (independently re-verified this tick via `read_file`): the per-pixel radiance output is `selectedRadiance * W` where `W = sumWeight / max(M * selectedTarget, 1e-6f)` and `M = min(M, gConstants.MaxM)` (line 204). With MaxM=30 and typical M=2.93 (log line 258), the per-pixel W distribution has std=0.17 — high per-pixel variance that averages to 1.09 mean. This variance in W is the compressor mechanism.

- **Empirical observation**: pre-temporal `gi_raw floats` at line 239 has std=0.0911-0.1196 (frame-7 dump, true per-pixel Sponza variance), post-temporal gi_raw floats at line 253 has std=0.0457 (frame-8 dump, 2× compressed). ReSTIR summary line 258: M mean=2.93 max=9.0, W mean=1.090. The **mechanism is exactly what the v173 plan describes**.

- **The fix is minimally invasive**: `MaxM=30.0f → 1.0f` at TWO lines (950, 1005). This forces M=1.0 per pixel, which means W = sumWeight / (1.0 * selectedTarget) ≈ sumWeight / selectedTarget. For typical ReSTIR target distributions where target ≈ 1.0 and sumWeight ≈ 1.0, **W ≈ 1.0** per pixel, eliminating the variance compressor without disabling the temporal pass.

- **Caveat from v1557** (already addressed in plan risk #2): MaxM=1 may make the spatial pass degenerate (3x3 merge of M=1 reservoirs is essentially a delta-blend). The plan documents this risk and the mitigation (bump NumCandidates 8→16). Good.

## Plan completeness

The plan is complete:
- ✓ Exact patch site: line 950 (`TC.MaxM`) and line 1005 (`SC.MaxM`)
- ✓ Rebuilt + run command
- ✓ Expected log stats with numerical predictions
- ✓ Step 7 fallback (revert + compound with v172 AmbientScale)
- ✓ Acceptance criteria mapped to user's 7 criteria
- ✓ Hypothesis refutation chain (v170/v171/v172 all REFUTED)
- ✓ Skill-validity check + HARD-ENV-FINDING for terminal-blocked runspace

Minor refinements the operator should consider:
1. **AmbientScale at line 802**: leave at 0.35f for the v173-only test; compound-fix later if needed
2. **`r_ReSTIR_NumCandidates` CVar**: do not bump preemptively; only bump if temporal-stability fails Step 4
3. **`HLVM_RGI_BYPASS=1` env-var**: tick1557's earlier hypothesis of bypass — verify it doesn't exist in this test (the search shows no `bBypass` flag gating the temporal call at line 963, so the env-var discriminator is NOT usable; the v173 numerical knob IS the discriminator)
4. **Skip v166 era test recipe** (`PENDING_TESTS_v166.md` lines 14-105): v166 cycle is closed (ALL_KEEP in v166 audit); don't re-run the prior recipe or you'll reset progress

## plan_fidelity_check

N/A — this is the planner stage. The plan itself correctly cites:
- Source code with line numbers (`ReSTIR_Temporal_cs.hlsl:194-211`, `TestReSTIR_GI_Temporal.cpp:950, 1005`)
- Empirical log evidence with line numbers (log lines 232/239/253/254/258)
- Prior v1557 finding + v172 plan-review recommendation
- Tick1548 SunLight refutation

**No plan deviation needed.** The patch proposal matches the v1557 analytical finding (tick1557 also suggested "MaxM=30→1" as one of three discriminator paths).

## Risks acknowledged

The plan's 6 risks are all addressed:
1. MaxM=1 over-weight per sample — mitigation: bump NumCandidates
2. MaxM=1 spatial degeneration — mitigation: bump NumCandidates
3. Frame-0 degeneracy — well-defined, no issue expected
4. Path-D ping-pong dependency — irrelevant to MaxM change
5. Sun lighting confirmed present (tick1548) — no lights add needed
6. Display mean shift — explicit Step 7 fallback

## Feedback for planner (FIX only)

None. The plan is KEEP as-is.

## Verdict

**KEEP.** The v173 plan closes the variance-compression bisect with a 2-character-pair patch that:
- Targets the actual source of the variance collapse (temporal W via MaxM cap), not a downstream proxy
- Is mathematically grounded (W=sumWeight/(M*selectedTarget) → W→1 as M→1)
- Is empirically verifiable (display std predicted 0.09-0.12 vs current 0.046)
- Is minimally invasive (no shader changes, no nvrhi fork changes, no cmake regen)
- Has clear compound/revert fallback paths documented
- Correctly supersedes v170 (ComposeDisplay), v171 (ACES saturation), v172 (no lights) hypotheses — all REFUTED by the line-232/239/253 log evidence

The plan completes the bisect. Operator-side recipe is concrete and 5-min total. Per HARD INVARIANT 6 (never silently exit), the rest of the v173 cycle (commit, review, tests, audit) is produced by the cron on operator-execution via the recipe in plan Step 2-6.

## Plan-criticer self-check

- [x] Verified the math from `ReSTIR_Temporal_cs.hlsl:194-211` directly via `read_file` (file-only this tick)
- [x] Verified all line numbers in TestReSTIR_GI_Temporal.cpp via independent `read_file` (line 950, line 1005)
- [x] Cross-referenced empirical evidence (log lines 232/239/253/254/258) — re-confirmed this tick
- [x] Cross-referenced prior findings: tick1548 (SunLight at lines 1958-1983) and tick1557 (temporal-collapse math) — both INTACT
- [x] Did NOT introduce contradictions with v172 plan-review (which REVISED to recommend v173 = bypass-temporal discriminator — this v173 IS that bypass via MaxM)
- [x] Did NOT silently skip the planner stage (v173 plan written first, then critiqued)
- [x] Surfaced the missing-CVar caveat (no `r_ReSTIR_EnableTemporal` exists, confirmed via `search_files` returning 0 hits and `FReSTIRPass.h` having no bBypass flag)

— plan-criticer, 2026-08-15, tick1567, file-only, single-profile host, terminal-blocked.
