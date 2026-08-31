# Pending Plan Review v138
- plan: docs/PENDING_PLAN_v138.md
- verdict: KEEP
- reviewer: plan-criticer (file-only single-profile mode)
- timestamp: 2026-07-31

## Design soundness

The plan correctly identifies a **logical chain error in tick 248's reasoning** and proposes a 1-line surgical fix to restore the discriminator that v17/v18 originally designed.

**Verification of the plan's claim** (file-only read of GIPathTracing.hlsl):

1. **Line 462**: `float3 worldPos = GBufferWorldPos[pixel].rgb;` — `worldPos` is loaded directly from `GBufferWorldPos` SRV. If the SRV returns zero, `worldPos == (0,0,0)`.

2. **Lines 472-479**: `bypassEarlyReturn` is computed as `debugModeEarly == 20u || 21u || 22u || 30u || 31u`. **Mode 6u is NOT in this list.**

3. **Lines 481-484**: `if (!bypassEarlyReturn && length(worldPos) < 0.001) { Output[pixel] = float4(0.0, 0.0, 0.0, 1.0); return; }` — this fires for mode 6 when `worldPos == (0,0,0)`, which is the SRV-broken symptom.

4. **Line 593+**: The debug-mode switch at line 593 NEVER executes for pixels where the early-return fired. Mode 6u's gradient write at line 608 is structurally unreachable when the SRV is broken.

5. **Line 738**: `Output[pixel] = float4(debugColor, avgFirstHitDist);` — the only path to write `debugColor` for mode 6 is past line 481.

The discriminator chain is **structurally identical** to the v128/v131 design intent for modes 20/21/22/30/31u (which are already in the bypass list). Adding `6u` follows the established pattern.

The plan also correctly notes that **v137's premise was invalidated** by this finding — mode 6 was not a discriminator post-v137 because it was masked by the early-return. Tick 248's analysis incorrectly concluded "mode 6 = no SRV read = clean UAV discriminator." In reality, mode 6 = reads GBufferWorldPos for `worldPos` length-check → masked by early-return → write doesn't happen → dump is zero regardless of UAV or SRV state.

## Plan completeness

The plan correctly identifies the parent-runspace discriminator recipe (run mode 6 after rebuild; if gradient shows, UAV was the only bug; if still zero, SRV is the bug and v139 is needed).

**One minor gap** (not a fix-blocker): the plan doesn't explicitly verify that `Output[pixel].w = max(Output[pixel].w, 0.99994f)` (line 750) runs for mode 6 even with `bypassEarlyReturn=true`. I verified this is true — the alpha sentinel is unconditional and runs after the debug-mode switch on line 593, regardless of bypass. Mode 6's dump will show: `(red gradient, 0, blue gradient, 254/255 alpha)`. The per-pixel gradient pattern is the discriminator; alpha=254 is the alive-sentinel.

**Another minor gap**: the plan doesn't address what happens if `bypassEarlyReturn=true` for mode 6 AND the debug-mode switch somehow doesn't fire for mode 6. This is structurally impossible (mode 6 is case 6u in the switch on line 593), so it's not a real concern.

## Feedback for planner (FIX only)

None — plan is acceptable as-is. Proceed to impler.

---

**Per `six-role-pipeline §Role #2 (plan-criticer)`, this is a file-only verdict based on the plan content + read_file verification of the cited file:line references (GIPathTracing.hlsl:460-484, 593, 608, 738, 750). The re-analysis that invalidates v137's premise is mechanically supported by direct file reads.**