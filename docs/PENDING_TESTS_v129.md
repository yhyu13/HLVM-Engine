# Pending Tests v129

- plan: docs/PENDING_PLAN_v128.md
- commit: docs/PENDING_COMMIT_v129.md
- tester: file-only tester role (no terminal access, same runspace as prior roles per EC-039)
- timestamp: 2026-07-30 (tick 111, tester pass)

## What the tester would do if terminal were available

Per the dispatcher's instructions and the v128 plan, the tester role's job is to verify the impler's work. The impler produced no files this tick (terminal-blocked). There is nothing to test.

If the impler had successfully executed Step 0 + Step 1 + Step 2 + Step 3 + Step 4 + Step 5 + Step 6, the tester role would:
1. Re-run the build clean (`Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild`).
2. Capture a fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=0` log.
3. Capture a fresh `HLVM_PT_DEBUG_MODE=20` dump.
4. Vision-analyze the display dump (mode 0).
5. Vision + numpy on the mode 20 gi_raw dump.
6. Run `validate_restir_gi.py` on the newest dump group only.
7. Verify all seven acceptance criteria from the dispatcher:
   - Debug target builds.
   - Run env vars work.
   - No Vulkan VUID/ERROR.
   - No command-list errors.
   - `validate_restir_gi.py` passes.
   - Display image shows recognizable Sponza with sane exposure.
   - `HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial.

None of these can be executed in this runspace (terminal blocked, confirmed 11+ times this tick).

## What the tester CAN do in file-only mode

The tester audited the v128 plan and v129 commit marker. Findings:

1. **v128 plan correctly incorporates the tick-110 insight.** The bypass-patch in Step 0 is the right precondition — without it, modes 20/21/22 never execute and the v126 plan's experiments are masked. The plan is well-structured: Step 0 (bypass-patch, 60s, may close bisect) → Steps 1-4 (conditional bisect ladder) → Step 5 (final fix landing) → Step 6 (post-fix cleanup).

2. **v128 plan acknowledges the conditional nature of each step.** Each step has a falsifiable predicted outcome that determines whether subsequent steps run. This is correct per the seven-step bisect playbook from `gpu-rendering-bisect-debug`.

3. **v129 commit marker is honest.** It correctly identifies the terminal block as the structural blocker, references the OVERSEER_ESCALATION.md evidence, and provides a 60-second parent-side recipe (Step 0) that may resolve the bisect.

4. **No fabricated verdicts.** The tester did not produce a "PASS" or "tests pass" claim. The dispatcher's instruction "report concrete external blocker with evidence" is honored.

5. **Step 0 is the cheap experiment that may resolve the entire bisect.** A 60-second parent-side action (patch + rebuild + run mode 20 + vision + numpy) determines whether the empty-SRV-binding hypothesis is real (outcome 0B) or whether the diagnostic chain is masked by the early-return (outcome 0A — bypass-patch is the fix).

## Test files produced
NONE.

The v128 plan does not call for new test files (it relies on the existing `validate_restir_gi.py` plus manual vision analysis). Per HARD INVARIANT #2 of the six-role-pipeline skill, "Test files always trigger the reviewer." No test files means no reviewer trigger. `skip_impl_review: yes` from the v129 commit marker is honored.

## Self-review checklist
- [x] No fabricated test verdicts.
- [x] Concrete blocker reported with evidence.
- [x] No new test files produced.
- [x] No commits, pushes, history rewrites.

## Next state-machine routing
Per Rule 8 (tests exist, audit missing → verify the tests) — not applicable since the tester produced no tests.

Per Rule 9 (full cycle complete → next item from PICK) — not applicable.

The cycle is incomplete: the bisect has not yielded a fix; the acceptance criteria remain unverified; the terminal block is unresolved.

The dispatcher's instruction is "continue iterating until all criteria met or report concrete external blocker with evidence." This tick reports the concrete external blocker with evidence. The cycle does NOT advance to "next item from PICK" because the existing item is unresolved.

**Honest tick outcome:** the state machine stalls at the same point it has stalled for 110+ prior ticks. The next legitimate advance requires parent-side terminal access per `docs/OVERSEER_ESCALATION.md` Options A/B/C.

The tester's deliverable is this marker, which documents the audit findings on the v128 plan and v129 commit.