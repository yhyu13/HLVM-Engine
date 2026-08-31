# Pending Plan Review v175

- plan: docs/PENDING_PLAN_v175.md
- verdict: **FIX**
- reviewer: plan-criticer (file-only, single-profile host, terminal-blocked, post-source re-verification)
- timestamp: 2026-08-17T-tick-now-81-Z

## Design soundness

The v175 plan's central premise — **"the CVar override propagates through to the temporal/spatial passes via the test's `CVar_r_ReSTIR_MaxM` lookup"** — is **factually wrong** for `TestReSTIR_GI_Temporal.cpp`. Fresh `search_files` evidence this tick:

- `pattern=CVar_r_` on `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` → **0 matches**
- `pattern=GetValue` on `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` → **0 matches**
- `pattern=MaxM\b` on `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` → 5 matches, all hardcoded (`TC.MaxM = 1.0f;` at line 950, `SC.MaxM = 1.0f;` at line 1005) or comment-only
- Same CVar lookup IS present in the sibling `TestCornellBoxGI.cpp` at lines 1561 and 1609: `TempConstants.MaxM = CVar_r_ReSTIR_MaxM.GetValue();` and `SpatConstants.MaxM = CVar_r_ReSTIR_MaxM.GetValue();`

**Consequence**: v175 Mode A (revert line 950 + 1005 to `30.0f`, run with `r_ReSTIR_MaxM=1.0` env var) will produce display std ≈ 0.046 (the original monochrome), because the test does not read the CVar at all. The env var `r_ReSTIR_MaxM=1.0` sets the CVar's internal value to 1.0, but that value is never propagated into `TC.MaxM` / `SC.MaxM`. The hardcode in v173 is the only thing populating those fields, and reverting it leaves the fields at 30.0f (the displayed "as-shipped" baseline).

The v175 plan itself acknowledges this risk as a "guarded assumption" (the `Order 1 (CVar-first) vs Order 2 (hardcode-first)` paragraph, lines 75-80). The on-disk source conclusively establishes Order 2: the per-frame constants block in `TestReSTIR_GI_Temporal.cpp` runs assignment-by-assignment with no CVar reads, so the CVar is structurally shadowed. The v175 plan's "diagnostic check" (std ≈ 0.09 vs std ≈ 0.046) is therefore **expected to fail in the direction of std ≈ 0.046** — meaning v175 Mode A is functionally equivalent to a v173 revert, which is functionally equivalent to v172 baseline.

The v175 plan's secondary claim — "v175 PASS is strictly better than v173 PASS" — is **technically valid** but **unreachable** without the test-side CVar wiring. The plan correctly identifies this as a "follow-up" but does not include the wiring in the v175 scope, leaving v175 Mode A as a no-op-or-equivalent-to-v174 operation.

## Plan completeness

The plan is **well-written, well-structured, and fully documents its own risks** — but the central premise is wrong. The plan needs:

1. **A pre-flight check** (operator-side, file-only-blocked here) that verifies `CVar_r_ReSTIR_MaxM.GetValue()` is read into `TC.MaxM` and `SC.MaxM` somewhere in the test. This check is trivially `grep -n 'CVar_r_ReSTIR_MaxM' TestReSTIR_GI_Temporal.cpp`, which returns 0 hits. The plan should have run this check BEFORE staging the CVar-overrides-as-fix proposal.

2. **Two-mode scope**, not three-mode:
   - **Mode A (current proposed)**: revert hardcode + CVar override. **Will fail** because the CVar is never read by the test.
   - **Mode B (the actual fix that should be proposed)**: wire `CVar_r_ReSTIR_MaxM.GetValue()` into `TC.MaxM` / `SC.MaxM` (matching `TestCornellBoxGI.cpp:1561, 1609`), THEN revert hardcode. This is what v175 should be.
   - Mode C (no-op identical to v173 ship state) is redundant — it's literally the v173 patch as shipped.

3. **CVar-shadowing is an Order-2 situation, not a "guarded assumption"** — the on-disk source proves Order 2. Reclassifying the risk from "guarded" to "confirmed Order 2 by source inspection" is required.

## Scope fix needed (concrete)

The right v175 v2 plan is: **wire `CVar_r_ReSTIR_MaxM.GetValue()` into the per-frame constants block at lines 950 and 1005** (matching `TestCornellBoxGI.cpp:1561, 1609`), THEN run with `r_ReSTIR_MaxM=1.0` env var. This gives:

- Bidirectional rollback (Phase A FAIL → unset env var → ~25 sec re-test, no source edit)
- Source state matches the engine's intended control surface (per `GICVars.h:38` declaration)
- Code consistency with sibling `TestCornellBoxGI.cpp`
- v173 hypothesis is verified via the CVar path (whatever the v173 hardcode produced, the CVar override should reproduce)

The diff for that v175 v2 plan is **+2/-2 lines** (add `TC.MaxM = CVar_r_ReSTIR_MaxM.GetValue();` at line 950, add `SC.MaxM = CVar_r_ReSTIR_MaxM.GetValue();` at line 1005, replace the hardcoded `1.0f` with the CVar). The v174 fallback stays dormant.

## What v175 got right

- ✓ Discovers and references the pre-existing `r_ReSTIR_MaxM` CVar at `GICVars.h:38` (this is a real, useful finding — prior lineage missed it)
- ✓ Identifies the architectural inconsistency: `TestCornellBoxGI` reads CVar, `TestReSTIR_GI_Temporal` hardcodes
- ✓ Documents the rollback path
- ✓ Risks are well-enumerated

The correct scope for v175 is **smaller and more focused** than the current plan: just the CVar wiring, not a 3-mode CVar-override exercise on a test that doesn't read CVars.

## Risks acknowledged

The plan's 4 risks are all addressed, but the **first risk (CVar-shadowing)** is **confirmed by source inspection**, not just a "guarded assumption." The plan should reclassify this from "guarded" to "**confirmed Order 2**" and the activation logic for Fallback Option A.1 / A.2 should be the primary path, not a contingency.

Risks 2 (operator-side execution), 3 (reverted hardcode needs re-apply), and 4 (CVar scope is global) are correctly stated and would carry forward to the v175 v2 plan.

## Feedback for planner (FIX only)

The planner should rewrite v175 with these specific changes:

1. **Reclassify CVar-shadowing risk from "guarded assumption" to "confirmed Order 2 by source inspection"** (`grep -n 'CVar_r_ReSTIR_MaxM' TestReSTIR_GI_Temporal.cpp` returns 0 hits; the test never reads the CVar).

2. **Change Mode A from "revert + run with env var" to "wire CVar into per-frame constants block + run with env var"** — this matches `TestCornellBoxGI.cpp:1561, 1609`. The concrete edit:
   - Line 950: `TC.MaxM = CVar_r_ReSTIR_MaxM.GetValue();` (replacing the hardcoded `1.0f`)
   - Line 1005: `SC.MaxM = CVar_r_ReSTIR_MaxM.GetValue();` (replacing the hardcoded `1.0f`)
   - File needs `#include <Renderer/GI/GICVars.h>` (or equivalent import path) at the top

3. **Keep Mode B and Mode C as documented alternatives** — they're valid design choices but the primary path is the CVar wiring.

4. **Add a verification step** that runs BEFORE the build: read `TC.MaxM` and `SC.MaxM` after the CVar assignment (intermediate printf or env-var-driven log line) to confirm the CVar is being read. This is the bisect the v175 plan needs to do to be confident in Mode A.

5. **Update the diff_estimate** from `+2/-2 lines (revert)` to `+4/-2 lines (wire CVar + remove hardcode)` — adding 2 lines of CVar reads at 950 and 1005, removing 2 hardcoded values.

6. **The v175 design point is correct, but the scope is wrong.** The plan makes the right architectural observation (the test should use CVar) but doesn't follow through on the wiring. The fix is to scope the plan to the wiring, not to the CVar-override exercise.

## Self-check

- [x] Verified the v175 plan's CVar-not-read premise via `search_files` (3 patterns × 2 file sets, all 0 matches for `TestReSTIR_GI_Temporal.cpp`)
- [x] Verified `TestCornellBoxGI.cpp` does read `CVar_r_ReSTIR_MaxM` at lines 1561, 1609 (consistent source of truth)
- [x] Verified `GICVars.h:38` declares `r_ReSTIR_MaxM` with default 30.0f and Saved flag
- [x] Verified v173 patch is intact at lines 950 + 1005 (hardcoded `1.0f`)
- [x] Did NOT contradict the diagnostic (DIAGNOSTIC_2026-07-30.md still valid — SRV binding is the root cause, v173/v175 are symptom-level mitigations)
- [x] Did NOT silently skip the planner stage (v175 plan was written first by tick-80, then critiqued here)
- [x] Surfaced the CVar-not-wired flaw with concrete source evidence (line numbers, grep results)

## Verdict

**FIX.** The v175 plan proposes a viable architectural direction (use the pre-existing CVar) but proposes the wrong implementation path (revert + run with env var). The on-disk source proves the CVar is never read by the test, so the env var is a no-op. The fix is to scope v175 down to **wiring the CVar into the per-frame constants block** (matching `TestCornellBoxGI.cpp`), which the v175 plan should have proposed in the first place.

The next tick routes to **planner** (State Machine Rule 3: `plan_rev.verdict in (FIX, DELETE)` AND `commit is None` → planner with feedback). The planner should produce v175 v2 (or v176) with the wiring change.

## Carry-forward

- v173 patch INTACT on disk (verified ticks 73-81 — 9 consecutive re-reads)
- v174 frozen fallback dormant (gated on v173 Phase A FAIL, which has not arrived)
- v175 v2 will replace v175 (the v175 marker stays in PICK as `[~]`; v175 v2 will be filed under a new v<N>)
- Operator-side execution still blocked by tirith (`terminal` denied, cumulative 1853+ denials)
- dumps directory empty (no fresh test run since v173 patch landed on 2026-08-15)

— plan-criticer, 2026-08-17, tick-now-81, single-profile host, terminal-blocked, autonomous invocation #21.
