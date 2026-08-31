# Pending Tests v171

- commit: docs/PENDING_COMMIT_v171.md
- plan: docs/PENDING_PLAN_v171.md
- runner: tester (file-only, single-profile host)
- timestamp: 2026-08-15T-tick1551-Z

## Test plan (operator-side, written for the human to execute)

The v171 commit proposes a 1-line patch (`DescGI.AmbientScale = 0.0f;`) in `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`. The tester role cannot empirically execute the required commands in this runspace (terminal blocked by tirith, see HARD-ENV-FINDING-1 below). This marker captures the test design the operator runs:

### Recipe (from `PENDING_PLAN_v171.md` and `PENDING_COMMIT_v171.md`)

1. **Precondition — Run 1 (mode=3 discriminator)**: confirm `primaryDirect` is per-pixel-varying BEFORE applying the patch. If primaryDirect is also broken (near-zero mean/std), the patch alone won't fix the image.
   ```bash
   HLVM_PT_DEBUG_MODE=3 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal
   # Check mode3 log: if mean ≈ 0.04, primaryDirect is broken too — escalate to Patch 3 (boost SunLight)
   ```

2. **Apply the patch**: `DescGI.AmbientScale = 0.0f;` in the per-frame Desc init block of `TestReSTIR_GI_Temporal.cpp`.

3. **Rebuild Debug**:
   ```bash
   ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
   ```

4. **Run with HLVM_PT_DEBUG_MODE=20** (GBufferMaterial SRV read from GI shader — discriminates binding-fix from ambient-fix):
   ```bash
   HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal
   # Check log + dump. Expected post-patch: gi_raw contains varied albedo (Sponza surfaces), NOT uniform (0,0,0).
   # If still uniform zero — the binding-set issue from DIAGNOSTIC_2026-07-30.md modes 20/21/22 is the residual bug.
   ```

5. **Run the standard display test**:
   ```bash
   HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal
   ```

6. **Grep log**:
   ```bash
   grep -E "VUID|ERROR|CommandList" Binary/Debug/TestReSTIR_GI_Temporal.log | wc -l   # expect 0
   grep -E "display floats:" -A1 Binary/Debug/TestReSTIR_GI_Temporal.log               # expect mean ≈ 0.30, std ≈ 0.18
   ```

7. **Validate the dump group**:
   ```bash
   python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
   # Expected: 6/6 PASS (was 3/6 FAIL pre-patch)
   ```

8. **Vision-check `display_frame8.png`**: expect recognizable Sponza walls with directional shadows.

## HARD-ENV-FINDING-1 (cron-verified this tick)

The tester role requires shell commands (`./Build.sh`, `HLVM_*` env-var run, `grep`, `python3`, `validate_restir_gi.py`, vision via `vision_analyze`). All are routed through `terminal`. **Terminal is blocked by tirith at the security-pattern gate.**

Probes this turn:
- `echo "terminal-probe-$(date +%s)" && ls ...` → `pending_approval: tirith:unknown`
- `cronjob list 2>&1 | head -50` → `pending_approval: tirith:unknown`
- `echo "tick1551-probe-$(date +%s)"` → `pending_approval: tirith:unknown`

This is the **1551st cumulative `terminal` denial** on this runspace. The host has been structurally file-only for the entire duration of this diagnostic chain (1550+ prior ticks confirmed the same).

## Verdict

**ALL_TESTS_BLOCKED.** The 8-step recipe above cannot be executed by the cron. The patch + recipe constitute a complete deliverable that an operator with terminal access closes in 2 rebuilds + ~3 minutes.

## Broken-pattern audit (file-only pre-screen)

- [ ] No `from-x-import-y` patch propagation bugs (this is C++, not Python — N/A)
- [x] No test-bug-in-itself: the recipe is identical to v170's PENDING_TESTS and the v171 plan's verify command
- [x] No source-incomplete-relative-to-test: the source patch is the proposal; the test reads from the build output, not from a re-implementation
- [x] No missing test isolation fixture: `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` env-vars are the isolation gate
- [x] No AsyncMock on sync function (N/A)

## What the operator must do

Apply the 8-step recipe above. Report back via the next cron invocation (or by editing PENDING_PICK.md to mark the v170/v171 cards DONE) once validator exits 0 with 6/6 PASS and vision confirms Sponza geometry.

— tester, tick 2026-08-15, file-only, single-profile host, terminal-blocked.
