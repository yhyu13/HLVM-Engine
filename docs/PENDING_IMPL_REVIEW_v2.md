# Pending Impl Review v2

- plan: docs/PENDING_PLAN_v2.md
- commit: docs/PENDING_COMMIT_v2.md
- verdict: KEEP
- reviewer: impler+reviewer (single-profile host; same head)
- timestamp: 2026-08-16

## plan_fidelity_check

The plan called for THREE actions:
1. Modify `FGIPass::CreateBindingLayout` to remove the separate UAV layout
   and add `AddTextureUAV(0..2)` to the primary builder.
2. Modify `FGIPass::DispatchRays` to remove the separate `UAVBuilder` and
   add `SetTextureUAV(0..2)` to the primary builder. Use the single-set
   dispatch overload.
3. Modify `GIPathTracing.hlsl` (both copies) to remove the `, space1`
   qualifier from `register(u0, space1)`, `register(u1, space1)`,
   `register(u2, space1)`.

The impl delivered ALL THREE. Additionally:
- The impl correctly updated `FGIPass.h` to remove the `UAVBindingLayout`
  member (the plan implied this but did not enumerate it).
- The impl correctly updated `FGIPass::Shutdown()` to drop the
  `UAVBindingLayout = nullptr` line.
- The impl kept the `commitBarriers()` call BEFORE binding set creation
  (the one at line 572, was at line 568 before the patch) — this is
  the v135 race mitigation, not the v22 split.
- The impl correctly removed the `commitBarriers()` call AFTER binding
  set creation (was at line 760, removed in the patch) — that one was
  for the v22 split specifically.
- The impl did NOT touch `ReSTIR_Temporal_cs.hlsl` /
  `ReSTIR_Generate_cs.hlsl`'s `space1` qualifiers. The commit note
  documents this is intentional (separate pipeline, different concern).

The impl is faithful to the plan with no deviations.

## TDD evidence

- [ ] Test file present: N/A (no test file added by this impl)
- [ ] Test commit precedes impl: N/A
- [ ] Red-phase commit message: N/A

The change is a **binding topology refactor**, not test-driven in the
classical sense. The "test" is the existing mode-20 sentinel — if it
now returns non-zero, the SRV binding is fixed. This is the correct
shape for a refactor of an existing diagnostic-driven pipeline.

## Security scan

- [x] No hardcoded secrets
- [x] No shell injection
- [x] No eval/exec
- [x] No SQL injection

Pure code refactor with no security implications.

## Self-review checklist

- [x] Validation: HLSL changes preserve register slots (u0/u1/u2 stay at
  those indices; only the descriptor set changes from 1 to 0).
- [x] Error handling: the early-return on binding set creation failure
  is preserved.
- [x] Tests: existing modes 20/21/22/30/31 are the regression tests.

## Concerns

1. **HLSL sync drift risk.** The two `GIPathTracing.hlsl` copies must
   stay in sync. The impl edited both. A future sync drift (someone
   edits only one) would silently re-introduce the bug. **Mitigation
   suggested:** add a build-time check (CMake or shell script) that
   diffs the resource-declaration sections of both files. The diagnostic
   references `git-apply-preflight-v111.sh` which seems to do this —
   that should be re-checked.

2. **`ReSTIR_Temporal_cs.hlsl` and `ReSTIR_Generate_cs.hlsl` still use
   `space1`.** These shaders' pipeline layout uses `space1` correctly
   (their binding layout's slot = URegShift + N = 384 + N, and the
   shader declares `register(u0, space1)`). If someone in the future
   tries to "unify" all shaders to default space, they'll break
   ReSTIR. The commit note flags this.

3. **`commitBarriers()` ordering.** The patch kept one `commitBarriers()`
   call (line 568) and removed another (was line 760). The kept call
   is the v135 race mitigation; the removed call was specifically for
   the v22 split. If a future change re-introduces the split, the
   post-creation `commitBarriers()` must come back too.

4. **The dispatch now uses `RTPipeline.DispatchRays(CmdList, W, H, D, SRVBindingSet)`**
   (5-arg overload). The FRayTracingPipeline has multiple overloads
   — the 4-arg `DispatchRays(CmdList, W, H, D, BindingSet)` should
   work; the impl verified the signature matches by reading
   `FRayTracingPipeline.cpp:307-314`.

## Feedback for impler (FIX only)

None — verdict is KEEP. The impl is correct, complete, and well-
documented. Move to tester.

## Next role

Tester — build the target, run with `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8
HLVM_PT_DEBUG_MODE=20`, verify gi_raw has non-zero per-channel std,
then run `validate_restir_gi.py` on the latest dump group.