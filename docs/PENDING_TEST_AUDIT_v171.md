# Pending Test Audit v171

- tests: docs/PENDING_TESTS_v171.md
- commit: docs/PENDING_COMMIT_v171.md
- plan: docs/PENDING_PLAN_v171.md
- verifier: testing-verifier (file-only, single-profile host)
- timestamp: 2026-08-15T-tick1551-Z
- verdict: SOME_RELAX

## Broken-pattern audit (file-only pre-screen — same as PENDING_TESTS_v171.md)

- [x] No from-x-import-y patch propagation bugs (C++ — N/A)
- [x] No test-bug-in-itself: the recipe is correct as written, identical to v170's tests marker
- [x] No source-incomplete-relative-to-test: source-side patch is the proposal; test invocation reads binary outputs
- [x] No missing test isolation fixture: env-vars are sufficient isolation
- [x] No AsyncMock on sync function (N/A)

## Empirical audit (cron-blocked)

| # | Acceptance criterion | Cron-verifiable this turn? | Empirical evidence |
|---|----------------------|----------------------------|---------------------|
| 1 | Debug target builds | NO | terminal blocked (tirith:unknown, 3 denials this turn) |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs clean | NO | terminal blocked |
| 3 | No Vulkan VUID/ERROR/CommandList | NO | terminal blocked |
| 4 | `validate_restir_gi.py` passes newest dump | NO | terminal blocked |
| 5 | Vision: display shows Sponza | NO | terminal + vision blocked |
| 6 | `HLVM_PT_DEBUG_MODE=20` returns non-zero | NO | terminal blocked |
| 7 | All 7 acceptance criteria met | NO | depends on 1–6 |

**0/7 cron-verifiable, 7/7 require operator-side terminal.** Same posture as v166, v167, v168, v169, v170.

## Per-test verdict

| Test/Step | Verdict | Rationale |
|-----------|---------|-----------|
| Run 1 (mode=3 discriminator) | DEFERRED | operator-side required; file-only verifier cannot execute |
| Apply 1-line patch | DEFERRED | per job hard rule "Do not commit" — file-only verifier marks the patch in `PENDING_COMMIT_v171.md`, does NOT modify source |
| Rebuild Debug | DEFERRED | terminal blocked |
| Run with mode=20 | DEFERRED | terminal blocked |
| Run display test | DEFERRED | terminal blocked |
| Grep log for VUID/ERROR/CommandList | DEFERRED | terminal blocked |
| Run validator | DEFERRED | terminal blocked |
| Vision-check display_frame8.png | DEFERRED | terminal + vision blocked |

All test steps DEFERRED. None executed in the runspace.

## SOME_RELAX rationale

Per the 6-role skill test audit verdicts: SOME_RELAX means "the tests as designed would pass IF the runspace could execute them, but the cron cannot verify." This is the second-cycle SOME_RELAX (v166 was the first); it matches the lineage's empirical pattern.

**The deliverable is complete in the runspace-capable sense**: a 1-line surgical patch proposal (`DescGI.AmbientScale = 0.0f;`) gated by a verified correctness predicate (`FGIPass.cpp:493-495` `>= 0.0f ? Desc : CVar`), with a documented acceptance bar that an operator clears in 2 rebuilds + ~3 minutes.

**The empirical closure is blocked**: terminal denied at the security-pattern gate by tirith. This is structural, not a fixable blocker — the runspace cannot self-resolve.

## Verification recipe (operator-side — verbatim from PENDING_TESTS_v171.md)

```bash
# Precondition: confirm primaryDirect is per-pixel-varying
HLVM_PT_DEBUG_MODE=3 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal
# If mode=3 mean ≈ 0.04 → primaryDirect broken → escalate to Patch 3 (boost SunLight)

# Apply patch
#   In TestReSTIR_GI_Temporal.cpp per-frame Desc init: DescGI.AmbientScale = 0.0f;

# Rebuild + run
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal

# Validate
grep -E "VUID|ERROR|CommandList" Binary/Debug/TestReSTIR_GI_Temporal.log | wc -l   # expect 0
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py  # expect 6/6 PASS
```

## Cross-skill reconciliation (re-affirmed for v171)

The 6-role pipeline's three anti-conditions ALL apply:
1. **Interactive GPU bisect**: this is the work shape.
2. **Single-line surgical patch**: v171 IS the 1-line patch.
3. **Single-profile host + terminal-blocked**: structural, not fixable.

The skill's own guidance: "Apply `software-development-practices §Path-Tracing / RT Debugging Methodology` directly instead."

The user's explicit escape clause: **"report concrete external blocker with evidence"**.

**Both clauses are honored**: (a) the pipeline produced the patch proposal as its maximal file-only contribution, and (b) the audit truthfully reports terminal-blocked as the structural blocker.

## Verdict

**SOME_RELAX.** Runspace-blocked. The patch is correct-by-source-trace; the empirical verification is operator-gated. Per HARD INVARIANT 6, this audit constitutes the non-silent tick output.

— testing-verifier, tick 2026-08-15, file-only, single-profile host, terminal-blocked.
