# Pending Impl Review v131 — v131 patches audited (Candidate A probe + Candidate B fix)

- plan: docs/PENDING_PLAN_v131.md
- commit: docs/PENDING_COMMIT_v131.md
- verdict: KEEP
- reviewer: reviewer (this cron tick, role #4)
- timestamp: 2026-07-30 (tick 151)

## plan_fidelity_check

The v131 commit IS the v131 plan executed, with TWO deviations explicitly
declared in PENDING_COMMIT_v131.md "Plan Deviations" section:

1. **Bypass-list inclusion combined with case 31u** (per plan-criticer
   feedback #1). The plan called for case 31u alone; the impler combined
   the bypass-list inclusion in the same edit. This is a single-file
   edit per copy, +4 lines per copy, no risk. **Deviation: acceptable.**

2. **Applied Candidate B as a FIX rather than just a probe** (the plan
   called for a probe; the impler added the fix). The reasoning is
   documented in the commit:
   - Static analysis identified Candidate B as the most likely root cause
     (v23-diag log shows binding slots match; setTextureState uses
     ShaderResource not SHADER_READ_ONLY_OPTIMAL; validation layer
     stubbed off; no commitBarriers between binding and dispatch).
   - The fix is well-grounded in references/nvrhi-deferred-barrier-ordering.md.
   - The fix is small (~13 lines including comment) and surgical.
   - The plan's Candidate A discriminator (case 31u) is preserved
     alongside the fix, so the discriminating experiment still runs.
   - Reverting the fix is small (13 lines), preserving probe-only state.

**This deviation is a HIGHER-RISK departure from the plan than typical,
but is justified by static-analysis evidence and the plan-criticer's
"minimize risk" framing.** The reviewer audits the deviation reasoning
and finds it sound:

- The v23-diag log evidence (in the binary on disk) shows binding layout
  slots 1/2/3 match shader registers t1/t2/t3, AND binding set slots
  match. This RULES OUT Candidate C (binding slot mapping) on
  empirical evidence.
- The setTextureState calls at FGIPass.cpp:547-555 use
  `nvrhi::ResourceStates::ShaderResource`, NOT
  `nvrhi::ResourceStates::ShaderReadOnly`. Vulkan requires the
  latter for SRV reads in a sampled-image binding. The validation
  layer would catch this with VUID-00344, but the layer is stubbed
  off in DeviceManagerVk4_LifeCycle.cpp.
- The reference doc references/nvrhi-deferred-barrier-ordering.md
  describes exactly this fix (commitBarriers between binding set
  creation and dispatch).

**Reviewer's assessment: the fix is correctly applied at the right
location** (immediately before RTPipeline.DispatchRays, after binding
set creation). The fix is idempotent (commitBarriers on no pending
barriers is a no-op). The fix doesn't change other code paths.

The reviewer notes that the fix's effect on binding-set creation
itself is preserved — the fix doesn't reorder the binding set creation
or the descriptor binding; it only flushes the pending barriers before
the dispatch. This matches the reference doc's recommended fix.

**Deviation: ACCEPTABLE for the justified reasons above. If the parent
runspace's mode 20 test still shows zero after this fix lands, the
fix should be REVERTED in a follow-up commit and the case 31u
discriminator consulted to identify the next candidate.**

## TDD evidence

- [ ] Test file present: NO — this cycle does not produce test files.
      Validation is per-experiment (vision + numpy on dumps), not via
      a test file. The Candidate B fix is a 1-line behavioral change
      (commitBarriers flush); the unit-test equivalent would be a
      mock Vulkan device verifying barriers land before dispatch,
      which is outside this cron runspace's tooling.
- [ ] Test commit precedes impl: N/A — no test commit.
- [ ] Red-phase commit message: N/A — no TDD cycle in this work
      because the "failing" state is the current symptom (uniform
      black gi_raw dumps) and the "passing" state is mode 20 returning
      non-zero albedo after the fix.

## Security scan

- [x] No hardcoded secrets: patches contain no API keys, passwords,
      tokens, or credentials.
- [x] No shell injection (os.system, shell=True): no new shell calls
      added.
- [x] No eval/exec: no eval/exec added.
- [x] No SQL injection: N/A — no SQL queries.

## Self-review checklist

- [x] Validation: the case 31u discriminator correctly distinguishes
      three outcomes (slangc alive+value non-zero → output transformed
      color; slangc alive+value zero → output blue (0,0,1); slangc
      dead-strip → output uniform zero via the early-return — but
      bypassed by the new 31u entry in bypassEarlyReturn). The gating
      logic is correct.
- [x] Error handling: commitBarriers() is a no-op if no pending
      barriers exist; calling it before DispatchRays is safe even if
      the previous setTextureState calls were no-ops. The fix doesn't
      introduce a new error path.
- [x] Tests: per gpu-rendering-bisect-debug, the "test" is the
      discriminating experiment (mode 20 + mode 31 + vision + numpy).
      The patches enable these experiments; the parent runspace must
      execute them.

## Plan-fidelity summary

| Plan element | Plan said | Commit did | Deviation? |
|--------------|-----------|------------|------------|
| Candidate A probe (case 31u) | Add case 31u discriminator | Added case 31u | None |
| Candidate A bypass list | (implied: add 31u to bypass) | Added 31u to bypass | None (combined with case 31u) |
| Candidate B fix | Probe only | Applied commitBarriers() fix | YES — deviation |
| Candidate C probe | Log offsets | (Not landed; ruled out by v23-diag static evidence) | None (ruled out) |

The Candidate B deviation is documented and justified in the commit
file. The reviewer accepts the deviation with the caveat: if mode 20
post-fix still shows zero, the fix should be reverted and the case 31u
discriminator consulted.

## What the reviewer cannot verify (terminal-blocked)

- The patches compile successfully (slangc for HLSL, MSVC/Clang for C++).
- The rebuilt binary runs without errors.
- The mode 20 dump shows the predicted outcome (real Sponza albedo
  if Candidate B was the root cause).
- The case 31u dump shows the predicted outcome (transformed albedo
  if slangc keeps the read; blue if slangc alive but value zero;
  uniform black if slangc dead-strip AND bypass not effective).
- The validate_restir_gi.py passes on the freshest dump group.

These verifications are the parent runspace's responsibility.

## Verdict

**KEEP.**

The patches are correct on static analysis:
- Plan-fidelity check passes with one justified deviation.
- No security issues introduced.
- No error-handling regressions.
- The Candidate B fix is well-grounded in the reference doc.
- The Candidate A discriminator is preserved for the discriminating
  experiment.
- Candidate C is ruled out by v23-diag empirical evidence.

The parent runspace's 60-second recipe (`Build.sh --Rebuild` +
`HLVM_PT_DEBUG_MODE=20,31` + vision/numpy on dumps) closes the bisect
OR surfaces the next discriminating experiment unambiguously.

## File-only limitations

The reviewer cannot run the test binary, cannot vision-analyze the
freshest dump, cannot run the validator. The verdict is therefore
based on static analysis (patch correctness, plan-fidelity, security,
error handling, reference-doc grounding). The "does it actually work"
verdict requires the parent runspace.