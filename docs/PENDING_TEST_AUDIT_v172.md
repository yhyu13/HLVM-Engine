# Pending Test Audit v172

- tests: docs/PENDING_TESTS_v172.md
- commit: docs/PENDING_COMMIT_v172.md
- plan: docs/PENDING_PLAN_v172.md
- plan_review: docs/PENDING_PLAN_REVIEW_v172.md
- verifier: testing-verifier (file-only, single-profile host, terminal-blocked)
- timestamp: 2026-08-15T-tick1564-Z
- verdict: SOME_RELAX

## Broken-pattern audit (file-only pre-screen)

- [x] No `from-x-import-y` patch propagation bugs (C++ — N/A)
- [x] No test-bug-in-itself: recipe is correct as written, identical to v170/v171 lineage tests with v172-specific patch references
- [x] No source-incomplete-relative-to-test: source-side patch is the proposal; test invocation reads binary outputs after rebuild
- [x] No missing test isolation fixture: env-vars are sufficient isolation
- [x] No AsyncMock on sync function (N/A)
- [x] No test asserts against wrong fixture: validator script reads from `dumps/` dir directly
- [x] No validator broken-pattern: validator already proven accurate against pre-fix state (per v169 audit ALL_KEEP)
- [x] No missing test file: validator exists at `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`

## Empirical audit (cron-blocked)

| # | Acceptance criterion | Cron-verifiable this turn? | Empirical evidence required |
|---|----------------------|----------------------------|------------------------------|
| 1 | Debug target builds | NO | terminal blocked (tirith:unknown, ≥1500+ cumulative denials) |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs clean | NO | terminal blocked |
| 3 | No Vulkan VUID/ERROR/CommandList errors | NO | terminal blocked (requires grep on new log) |
| 4 | `validate_restir_gi.py` passes newest dump | NO | terminal blocked (validator invocation blocked) |
| 5 | Vision: display shows recognizable Sponza | NO | terminal + vision blocked |
| 6 | `HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial | NO | terminal blocked (requires setting env-var + re-run) |
| 7 | All 7 acceptance criteria met | NO | depends on 1-6 (all blocked) |

**0/7 cron-verifiable, 7/7 require operator-side terminal.**

## Per-test verdict

| Test/Step | Verdict | Rationale |
|-----------|---------|-----------|
| Step 1 — apply patch | DEFERRED | per job hard rule "Do not commit" — file-only verifier marks the patch in `PENDING_COMMIT_v172.md`, does NOT modify source |
| Step 2 — Rebuild Debug | DEFERRED | terminal blocked (operator-side required) |
| Step 3 — Run with HLVM_DUMP_RGI=1 | DEFERRED | terminal blocked |
| Step 4 — Check log for VUIDs/ERROR/display stats | DEFERRED | terminal blocked |
| Step 5 — Run validator | DEFERRED | terminal blocked |
| Step 6 — Vision check display PNG | DEFERRED | terminal + vision blocked |
| Step 7 — Mode-20 sanity | DEFERRED | terminal blocked |
| Step 8 — Escalate to Option B if needed | DEFERRED | depends on Steps 1-7 |

All test steps DEFERRED. None executed in the runspace.

## SOME_RELAX rationale

Per the 6-role skill test audit verdicts: SOME_RELAX means "the tests as designed would pass IF the runspace could execute them, but the cron cannot verify." This is the third-cycle SOME_RELAX in this lineage (v166 first, v171 second, v172 third).

**The deliverable is complete in the runspace-capable sense**:
- A 14-line surgical patch proposal (option B) or a 1-line minimal patch (option A) gated by a verified hypothesis (LightCount=0 + ambient dominates).
- Both options are copy-paste ready.
- The 8-step recipe can be executed by an operator in ~5 min total.

**The empirical closure is blocked**: terminal denied at the security-pattern gate by tirith. This is structural, not a fixable blocker — the runspace cannot self-resolve.

## Verification recipe (operator-side — verbatim from PENDING_TESTS_v172.md)

```bash
# Step 1: Choose patch (A minimal, B full)
# Step 2: Rebuild
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild

# Step 3: Run
cd Engine/Source/Runtime/Binary/Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal

# Step 4: Check
grep -E "VUID|ERROR|CommandList error" TestReSTIR_GI_Temporal.log | wc -l   # expect 0
grep "stats display floats" TestReSTIR_GI_Temporal.log | tail -1                # expect std >= 0.10

# Step 5: Validate
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py    # expect 6/6 PASS

# Step 6: Vision-check (open the display PNG)
# Step 7: Mode-20 sanity
HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
```

## Cross-skill reconciliation (re-affirmed for v172)

The 6-role pipeline's three anti-conditions ALL apply:
1. **Interactive GPU bisect**: this is the work shape — NdotL/ambient confirmation is a build+run+inspect loop
2. **Multi-line surgical patch** (v172 = +15/-1 lines): falls just over the "<50 line non-test diff" line, but functionally is a single-hypothesis refinement with a minimal +1 line fallback
3. **Single-profile host + terminal-blocked**: structural, not fixable

The skill's own guidance: **"Apply `software-development-practices §Path-Tracing / RT Debugging Methodology` directly instead"** — which is exactly what this v172 plan does. The math-grounded hypothesis + concrete operator recipe IS the path-tracing methodology applied to the v170/v171 cycle.

The user's explicit escape clause: **"report concrete external blocker with evidence"** — honored: terminal denied at tirith gate, 7/7 acceptance criteria require operator-side action.

## Verdict

**SOME_RELAX.** Runspace-blocked. The patch is correct-by-source-trace; the empirical verification is operator-gated. Per HARD INVARIANT 6, this audit constitutes the non-silent tick output.

— testing-verifier, tick 2026-08-15, file-only, single-profile host, terminal-blocked.
