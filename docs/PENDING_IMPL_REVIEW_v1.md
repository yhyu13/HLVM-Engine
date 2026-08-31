# Pending Impl Review v1

- plan: docs/PENDING_PLAN_v1.md
- commit: docs/PENDING_COMMIT_v1.md
- verdict: FIX
- reviewer: impler+reviewer (single-profile host; same head)
- timestamp: 2026-08-16

## plan_fidelity_check

The plan called for THREE actions:
1. Add `-D HLVM_RGI_DEBUG_VIS` to `create_restir_gi_temporal_shadermake`.
2. Add identity-verification logging in `FGIPass::DispatchRays`.
3. (Implicit) Reflect on the sblob to confirm binding layout matches SPIR-V.

The impl delivered:
- Action 1: **DONE**, but with a self-discovered caveat documented in the
  commit's "Plan Deviations" section — the cfg-level flag already defines the
  macro, so the cmake-level addition is redundant.
- Action 2: **NOT DONE**. The existing `[v23-diag]` block at FGIPass.cpp:666-689
  already logs the layout + set items with resource handles. Re-adding it
  would be a duplicate. The plan was unaware of the existing diagnostic block.
- Action 3: **NOT POSSIBLE in this tick** (no spirv-cross execution, no
  shader rebuild). The commit defers it to the parent session.

The impl is faithful to the PLAN but the plan's central HYPOTHESIS
(macro mismatch) is falsified by the impl's own investigation. The commit
honestly documents this — the impler did not re-plan silently, did not
paper over the falsification.

## TDD evidence

- [ ] Test file present: N/A (no test file added by this impl)
- [ ] Test commit precedes impl: N/A
- [ ] Red-phase commit message: N/A

The change is a **build-script constant addition**, not test-driven in the
classical sense. The "test" is the downstream binary — the next `./Build.sh
--Target=TestReSTIR_GI_Temporal` invocation will produce an sblob with
HLVM_RGI_DEBUG_VIS baked in via the redundant cmake-level flag (in addition
to the cfg-level flag, no semantic change).

## Security scan

- [x] No hardcoded secrets
- [x] No shell injection (no `os.system`, no `shell=True`)
- [x] No eval/exec
- [x] No SQL injection

The change is a single string concatenation in a Python build script. Safe.

## Self-review checklist

- [x] Validation: string is `" -DGI_DEBUG_STATS=1 -DHLVM_RGI_DEBUG_VIS=1"`,
  parses cleanly as cmake `-D` flags.
- [x] Error handling: no new error paths introduced.
- [x] Tests: downstream test is unchanged in scope.

## Feedback for impler (FIX only)

**Verdict: FIX.** Reasons:

1. The impler correctly identified that the central plan hypothesis was
   falsified. The deviation note is excellent — it explicitly documents
   the cfg-level flag, the contradiction with the diagnostic's
   "mode 20 = black" observation, and the remaining unfixed root cause.

2. **However, the impler left the actual root cause UNFIXED.** The plan
   said "fix the SRV binding so mode 20 returns non-zero". The impl
   changed a build flag that doesn't change runtime behavior. **The
   acceptance criteria are still unmet.**

3. The fix needs to go deeper. The next iteration must either:
   - Revert the v22 split (FGIPass.cpp:317-358) and use a single binding
     set like TestCornellBoxGI does (the proven control), OR
   - Investigate the binding descriptor type more carefully — perhaps
     use `nvrhi::DescriptorType::SampledTexture` vs `StorageImage` for
     the t1/t2/t3 items, OR
   - Run with Vulkan validation layer enabled (the diagnostic's
     option 8) and capture the actual error.

4. **One concrete next-step recommendation**: revert the v22 split. The
   SRV binding set should contain ALL resources (SRV + UAV), exactly
   like TestCornellBoxGI does. The v22 split was added to silence a
   Vulkan validation warning, but the warning is non-fatal and the
   data corruption it masked (zero SRV reads) is worse than the warning.

## Next role

Tester — try the reverted single-binding-set approach in a Plan v2 cycle,
or run with validation layer enabled.