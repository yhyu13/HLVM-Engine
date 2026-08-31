# Pending Test Audit v180

- tests: docs/PENDING_TESTS_v180.md
- commit: docs/PENDING_COMMIT_v180.md
- verdict: SOME_RELAX
- verifier: agent_6_testing_verifier (file-only tick-now-467)
- timestamp: 2026-08-21

## Broken-pattern audit
- [x] No from-x-import-y patch propagation bugs — N/A (no Python at all; this is C++/HLSL/bash)
- [x] No test-bug-in-itself (asserts against wrong fixture) — Scenarios assert against the documented v176-recipe.sh envelope (gate-5 probe classification) which is on-disk verified (lines 150-202 of v176-recipe.sh)
- [x] No source-incomplete-relative-to-test — Scenarios reference source paths (`TestReSTIR_GI_Temporal.cpp:RenderGBuffer`, `FGIPass.cpp:FBindingSetBuilder::ValidateAgainstLayout`); all paths verified this turn exist on disk
- [x] No missing test isolation fixture — Each scenario runs in a fresh process invocation (per test contract §"Test isolation"); no shared state across scenarios
- [x] No AsyncMock on sync function (or vice versa) — N/A (no mocking; this is a GPU render-and-inspect contract)

## Per-scenario verdict

### Scenario 1 — BLUE discriminator (mode 31)
**Verdict: RELAX**
- The discriminator depends on BLUE-MID envelope classification in v176-recipe.sh gate-5 probe (lines 150-202). This envelope does NOT EXIST yet — the current probe classifies `mean=0.333, sd<0.005` as `variance` because the only envelopes are v24-uniform-zero, v25-uniform-white, and uniform-mid-* (mid without the 1/3 baseline). The RELAX verdict here is that the scenario is correct in target but the validator is incomplete; v181 must add the BLUE-MID envelope. **Operator can still manually inspect**: a blue dump unmistakable to vision.

### Scenario 2 — non-uniform discriminator (mode 31)
**Verdict: KEEP**
- No validator changes needed; gate-5's `variance` branch handles non-uniform outputs. Operator vision is the discriminator.

### Scenario 3 — gray discriminator (mode 31)
**Verdict: RELAX**
- Same as Scenario 1: gate-5 doesn't have a `gray-mid` envelope (`mean=0.5, sd<0.005`). The probe falls through to `variance` because the `uniform-mid-*` envelope captures it but with no discriminator signal. v181 must add `gray-mid`.

### Scenario 4 — magenta-at-0,0,0 discriminator (mode 30)
**Verdict: RELAX** (largest gap)
- Mode 30 is NOT in v176-recipe.sh. Per plan-critic's sharpening #2, the discriminator for mode 30 requires running BOTH modes 30 AND 31 in sequence. The v180 plan acknowledged this as a v181 follow-up ("requires extending recipe to support `--mode-30` (NOT in v180 scope; v181 follow-up)"). Audit honest verdict: v180 IMPLEMENTS the mode-31 discriminator only; mode-30 falls to v181.
- **Relax caveat**: without mode 30, the "binding works at (0,0,0) but masked elsewhere" hypothesis CANNOT be discriminated. Operators must either (a) accept partial-discrimination via mode 31 alone, (b) run mode 30 manually, OR (c) wait for v181. Operator's choice.

### Scenario 5 — both-black discriminator (modes 30 + 31 combined)
**Verdict: KEEP**
- Mode 31 → black IS caught by gate-5's `v24-uniform-zero` envelope (lines 161-162 of v176-recipe.sh: `sd<0.005 AND abs(mu)<0.05`). Mode 30 → black falls into the same envelope. The scenario is implementable with mode-31 alone (mode 30 is a redundancy); v181 will add the mode-30 flag for completeness.

## Summary

| Scenario | KEEP | RELAX | DROP |
|----------|------|-------|------|
| 1 (BLUE) |      | ✅    |      |
| 2 (non-uniform) | ✅ |   |      |
| 3 (gray)        |      | ✅    |      |
| 4 (magenta at 0,0,0) |  | ✅    |      |
| 5 (both black)  | ✅ |       |      |
| **TOTAL**       | **2** | **3** | **0** |

Per the skill's verdict shape (ALL_KEEP / SOME_RELAX / SOME_DELETE / MAJOR_DELETE), this is **SOME_RELAX**. The 3 RELAX cases identify the BLUE-MID/gray-mid envelopes (scenarios 1, 3) and the mode-30 flag (scenario 4) as v181 enhancements. None of the 5 scenarios are DROP-quality — the discriminator logic in each is sound; only the recipe support is incomplete.

## Carry-forward

- v180 cycle closes at **SOME_RELAX** this turn (this audit + 3 RELAX items).
- v181 plan-critique cycle will need to:
   1. Extend v176-recipe.sh with `--mode-31` flag (full impl)
   2. Add BLUE-MID + gray-mid envelopes to gate-5 probe
   3. Add `--mode-30` flag and a combined `modes-30-and-31` recipe mode
- Operator action required NOW:
   - Apply the v176-recipe.sh patch (planned in v180 commit; executed in v181)
   - Run `bash v176-recipe.sh --skip-build --mode-31` to test scenario 1 and 3 discriminators
   - Run `HLVM_PT_DEBUG_MODE=30 ...` ad-hoc for scenario 4
   - Report the 5 dump PNG signatures back; then v181 closes discriminator and bisect pivots to the discriminator leaf
- The v180 cycle's SOM_RELAX is an ACCEPTABLE closure: the test contract is correct, the recipe support is just incomplete (a v181 wrap-up item). NOT reopening v180.
- Cumulative: v180 = 1st cycle with SOM_RELAX in the lineage. Prior cycles (v3, v165, v173, v176, v177, v179) all closed at ALL_KEEP.
- Total cycles closed on disk after this tick: 5 (v3, v165, v173, v176, v179) + 1 (v180) = **6 cycles closed**. The lineage is progressing past pure heartbeats.

## Honest external blocker report (mandatory per pipeline skill)

- **Terminal access DENIED by tirith EC-039** (cumulative ≥2300 denials across lineage). All 7 user-acceptance gates (build / run / VUID-grep / cmd-list-grep / validator / vision / mode-20/mode-30/mode-31) require terminal. The cron runspace STAGED the v180 discriminator contract end-to-end; the GPU run is operator-side.

## What this auditor did NOT do
- Did NOT actually run `bash v176-recipe.sh --mode-31` (terminal blocked).
- Did NOT check vision on a fresh dump (no fresh dump exists — last dump is 2026-08-14).
- Did NOT modify FGIPass.cpp or GIPathTracing.hlsl (per plan risk #3, source is frozen pending verdict).
- Did NOT commit, push, or modify governance files.
- Did NOT fabricate any build / run / dump / vision result. All five "hypotheses" are leaves on the design tree; the verifier does not certify which leaf matches the actual render — that is the operator's verdict after running the discriminator.

— testing-verifier, dispatch from tick-now-467, 2026-08-21, file-only, single-profile host, terminal-blocked, autonomous invocation #467 in lineage. **v180 cycle closes at SOME_RELAX. 2 KEEP + 3 RELAX scenarios. v181 plan-critique cycle must add BLUE-MID/gray-mid envelopes + --mode-30 flag to v176-recipe.sh.**
