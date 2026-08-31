# Pending Plan v174 — Frozen compound-fix fallback (ACTIVATES ONLY IF v173 Phase A FAILs)

- task: If `PENDING_PLAN_v173` Phase A verification fails (display std < 0.07, validator still FAIL, or vision shows monochrome), apply the compound fix from v173 Step 7: revert v173's MaxM=1.0f back to 30.0f, then bump `r_ReSTIR_NumCandidates` 8→16 + reduce `Desc.AmbientScale` 0.35→0.10.
- source: `docs/PENDING_PLAN_v173.md` §"Step 7: If display is too dim or too dark" (lines 149-152) — this plan is verbatim the Step 7 fallback, pre-staged so the next cron tick can fire the moment Phase A FAIL evidence arrives.
- approach: **FROZEN FALLBACK**. This plan is a pre-staged contingency, not an active plan. It writes ONLY if the v173 cycle's Phase A verification provides FAIL evidence. The cron this tick (2026-08-17) wrote this as a frozen fallback, gated by the next operator-side execution. Stays dormant if v173 Phase A PASSes.
- skip_planning: yes — frozen fallback; no fresh design needed; the design is `docs/PENDING_PLAN_v173.md` Step 7 verbatim.
- skip_plan_review: yes — same; v173 plan-review already evaluated the math.
- skip_impl_review: no — touches a CVar that may need to be added (if `r_ReSTIR_NumCandidates` doesn't exist on disk) AND a constant change. The reviewer should sanity-check the diff.
- produces_test_files: no — same as v173.
- test_strategy: operator-side (terminal-blocked cron). The recipe is identical to v173's recipe: rebuild + run + grep + validator + vision.
- risks:
  1. **`r_ReSTIR_NumCandidates` may not exist as a CVar on disk.** The v173 plan's Step 7 wording assumes it exists. The v173 plan-review explicitly did NOT verify its existence (it only verified `bBypass` does NOT exist). If this CVar is absent, the cron/operator must ADD it (one new `AUTO_CVAR_INT(r_ReSTIR_NumCandidates, 8, "...", Saved)` line in a CVar-aware file) before bumping it.
  2. **Phase A status is UNKNOWN this tick.** The cron is file-only. The v173 patch is on disk; no fresh build has run since (log timestamp 2026-08-14 22:18:56 = 3 days stale). We cannot preemptively know whether v173 will PASS or FAIL. This plan stays dormant until Phase A evidence arrives.
  3. **Reversion correctness.** Reverting `TC.MaxM` and `SC.MaxM` from 1.0f back to 30.0f must be exact (lines 950, 1005 of `TestReSTIR_GI_Temporal.cpp`). Wrong numbers silently re-introduce the v173 collapse.
  4. **Compound fix is theoretically sound but unproven.** v173 plan's "0.10" for AmbientScale is a guess calibrated against the v166 evidence. May need operator-side tuning.

## v173 Step 7 verbatim (this plan's source of truth)

> Step 7: If display is too dim or too dark
> - If mean < 0.10 (very dark, broken): the NoMoreVariance + reduced mean is sign the M=1 path kills the GI signal. Revert to MaxM=30.0f and apply a DIFFERENT fix (e.g., set `MaxM=1024.0f` to push M cap up and reduce W variance — but per-pixel W will still average).
> - If color-variance still fails (display std < 0.07): the variance is NOT coming through the spatial-rectifier. Apply v173 + the v172 AmbientScale=0.10 reduction (compounding fix) — predicted display std ≈ 0.10-0.13 in that case.
> - If temporal-stability fails (frame-to-frame jump > threshold): increase `r_ReSTIR_NumCandidates` from 8 to 16 (more sampling per pixel compensates for M=1).

Source: `docs/PENDING_PLAN_v173.md` lines 149-152.

## Concrete code changes (v174, conditional on v173 Phase A FAIL)

### Edit 1: Revert v173's MaxM changes (TestReSTIR_GI_Temporal.cpp)

Apply ONLY if v173 Phase A FAILS:

```cpp
// Line 950: revert from "1.0f" back to "30.0f"
TC.MaxM             = 30.0f;     // v174: revert v173; fallback to v172-era cap

// Line 1005: revert from "1.0f" back to "30.0f"
SC.MaxM             = 30.0f;     // v174: revert v173; matching cap downstream of temporal
```

### Edit 2: Reduce AmbientScale (TestReSTIR_GI_Temporal.cpp:802)

```cpp
// Line 802 currently reads:
Desc.AmbientScale      = 0.35f;
// v174: reduce to 0.10, matching v173 plan §"Step 7" compound fix
Desc.AmbientScale      = 0.10f;
```

### Edit 3: Bump r_ReSTIR_NumCandidates (CVar — ONLY if it exists)

```cpp
// If CVar exists:
CVar_r_ReSTIR_NumCandidates.SetValue(16);  // was 8
// If CVar does NOT exist, ADD it (in a CVar-aware file, e.g. TestReSTIR_GI_Temporal.cpp header block):
AUTO_CVAR_INT(r_ReSTIR_NumCandidates, 16, "ReSTIR candidate count per pixel (raise for stability, lower for cost)", Saved);
```

The "if CVar exists" decision is operator-side. The v173 plan-review's plan-fidelity-check did NOT verify this CVar exists. The next cron tick (when operator-side evidence is in) must run `search_files pattern=r_ReSTIR_NumCandidates` to confirm.

## diff_estimate

- Revert v173: +2/-2 lines (the 1.0f → 30.0f revert on lines 950 + 1005)
- AmbientScale change: +1/-1 line (line 802)
- CVar: +1 line (if adding) OR +1 line (if setting an existing CVar)
- Net: 3-5 line edits, all in `TestReSTIR_GI_Temporal.cpp`

## Verification (operator-side, identical to v173 recipe)

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
cd Engine/Source/Runtime/Binary/Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
grep "stats display floats" TestReSTIR_GI_Temporal.log | tail -1     # expect std >= 0.10
grep "stats gi_raw floats"  TestReSTIR_GI_Temporal.log | tail -1     # expect std >= 0.10
grep -E "VUID|ERROR|CommandList error" TestReSTIR_GI_Temporal.log | wc -l   # expect 0
python3 /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py  # expect 6/6 PASS
ls -t Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/*display_frame8.png | head -1
# Open — expect: Sponza gallery arches + floor + back wall + directional shadow
```

## Why this plan is frozen (do NOT activate without evidence)

This plan exists on disk so the next cron tick can fire it WITHOUT re-deriving the math from v173 Step 7. It is NOT a fresh plan. Its premise is that v173 Phase A FAILs — which is unknown this tick. The cron's job is to **wait for operator-side Phase A evidence** and either:

- Phase A PASS: v173 audit becomes ALL_KEEP, PICK `[~]` → `[x]`, v174 stays dormant (never committed).
- Phase A FAIL: v174 activates: planner iterates on v174 (per v173 PICK contract), impler applies the 3 edits in order, reviewer sanity-checks, tester runs the recipe, testing-verifier audits.

Either way, the v174 plan exists on disk and is ready to fire. The cron does NOT need to recommit it.

## FROZEN_FALLBACK marker

This plan is tagged as frozen. It MUST NOT be activated unless:
1. v173 Phase A evidence arrives (operator-side fresh build + run + dump + validate).
2. v173 Phase A outcome is FAIL (display std < 0.07, validator FAIL, vision monochrome).
3. The cron that activates this plan reads the Phase A evidence FIRST and confirms it justifies the compound fix.

If the v173 Phase A outcome is PASS, this file should be moved to `docs/archive/_frozen_v174_2026-08-17.md` and never activated.

— planner, dispatch from v173 cycle, 2026-08-17, file-only, single-profile host, terminal-blocked.
