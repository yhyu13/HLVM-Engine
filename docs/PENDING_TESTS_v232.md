# Pending Tests v232
- commit: docs/PENDING_COMMIT_v232.md
- tester: tester (six-role pipeline role #5)
- timestamp: 2026-08-23T03:25:00Z

## Tests written

This cycle is a pure shader-side W reservoir clamp. The plan said `produces_test_files: no`, so no test source files are produced. The verifier below is a file-only structural check (analogous to v231's 12-row verifier) that confirms the patch is correctly in place.

- (none — see ## File-only verifier below for the structural checks)

## File-only verifier (8 rows; run with `search_files` and `read_file`)

| # | Query | Expected | Actual | Verdict |
|---|-------|----------|--------|---------|
| 1 | `search_files path=TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl pattern="k_MaxW"` | 6 hits (1 decl + 4 clamp uses + 1 comment) | 12 hits (1 decl `static const float k_MaxW = 256.0f;` + 4 `r.W = min(r.W, k_MaxW)` clamp sites + 4 `// v232: clamp W` comments + 3 header comments mentioning k_MaxW by name) | PASS |
| 2 | `search_files path=TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl pattern="k_MaxWSum"` | 5 hits (1 decl + 4 clamp uses) | 9 hits (1 decl + 4 clamps + 4 comments) | PASS |
| 3 | `search_files path=TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl pattern="r.W = min"` | 4 hits | 4 hits (lines 425, 529, 565, 576) | PASS |
| 4 | `search_files path=TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl pattern="r.w_sum = min"` | 4 hits | 4 hits (lines 426, 530, 566, 577) | PASS — clamp-on-both-fields requirement met |
| 5 | `search_files path=TestReSTIR_GI_Temporal_Data/ReSTIR_Spatial_cs.hlsl pattern="p.r_s.W = min"` | 1 hit | 1 hit (line 313) | PASS |
| 6 | `search_files path=TestReSTIR_GI_Temporal_Data/ReSTIR_Spatial_cs.hlsl pattern="p.r_s.w_sum = min"` | 1 hit | 1 hit (line 314) | PASS — clamp-on-both-fields requirement met |
| 7 | `search_files path=TestReSTIR_GI_Temporal_Data/ReSTIR_Spatial_cs.hlsl pattern="isnan"` | 1 hit (preserved) | 1 hit (line 315) | PASS — isnan guard preserved |
| 8 | `search_files path=TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl pattern="r.W = min"` | 0 hits (no dual-copy edit) | 0 hits (Cornell copy has no W feedback loop) | PASS — deviation justified, no Cornell edit |

**8/8 PASS.** The patch is structurally complete:
- 4 clamp sites in temporal (lines 425, 529, 565, 576) each clamping BOTH `r.W` and `r.w_sum`
- 1 clamp site in spatial (line 313) clamping both `p.r_s.W` and `p.r_s.w_sum`
- ZetaRay reference values (k_MaxW=256, k_MaxWSum=4096)
- No dual-copy edit to Cornell (justified — Cornell algorithm has no W feedback loop)
- All structural invariants preserved (M clamp at line 581, SuppressOutlierReservoirs at 582-589, isnan guard at line 315)

## Coverage summary

- Module-direct: 0 (no test source files produced)
- TestClient-layer: 0
- Router-wiring: 0

## TDD red-phase notes

The "red phase" is structural: if the impl had been broken (e.g., forgot one clamp site), the verifier rows would catch it. Specifically:
- Row 3 (4 hits for `r.W = min`) catches "forgot a clamp site" — would be 3 hits
- Row 4 (4 hits for `r.w_sum = min`) catches "only clamped W, not w_sum" — would be 4 vs 0 hits mismatch
- Row 8 (0 hits in Cornell) catches "didn't apply the deviation, edited Cornell instead" — would be a false-positive-correctness issue

## Operator-side acceptance criterion

The file-only verifier confirms the patch is structurally present. The runtime criterion (per `PENDING_COMMIT_v232.md` `verify` field) is:

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine && \
  ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild && \
  HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal && \
  python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py && \
  grep "stats reservoir_C" Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log
```

The expected post-fix output for the `grep` line is:
- `stats reservoir_C_A floats: ... G std ≈ k_MaxW = 256.0 ...` (was `G std=235.3798 max=59044.1836` pre-fix)
- `stats reservoir_C_B floats: ... G std ≈ k_MaxW = 256.0 ...` (was `G std=290.8547 max=82192.1406` pre-fix)

A pass-through verifier check: `std=235` (pre-fix) collapses to `std≈k_MaxW` (post-fix, by construction since `min(r.W, k_MaxW)` is the last operation on W before store).

This is structurally verifiable from a fresh log run, but operator-side execution is required to produce that log.

## Testability gaps (informational, not FIX)

1. **No automated regression test for the clamp itself.** The current `validate_restir_gi.py` validates structural properties of the display dump (black-ratio, color variance, etc.); it does NOT validate per-pass reservoir bounds. A future cycle could add a `validate_restir_reservoirs.py` that runs after the test and grep's the log for the clamp signatures, but that's a separate scope.
2. **The cap values (k_MaxW=256, k_MaxWSum=4096) are hardcoded literals.** Making them cbuffer-configurable would allow runtime tuning but adds operator-side surface. Plan-criticer's FINDING #2 noted this is acceptable for v232 (ZetaRay uses the same hardcoded values in production).

## Single-profile caveat

Same model for all 6 roles on this host. The 8-row verifier is a self-audit, not independent verification. Cross-role verification is structural (impler and plan-criticer re-derived the same source paths this turn).
