# Pending Plan Review v190

- plan: docs/PENDING_PLAN_v190.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-537)
- timestamp: 2026-08-30

## Design soundness

The plan's load-bearing claim is that card D's prescribed remedy
(`CommandList->commitBarriers()` replacing the dispatch) is a no-op, because the
flush the pass is retained for comes from `setComputeState`, not from
`dispatch`. **I re-derived this from nvrhi source myself rather than accepting
the planner's quotation**, and it holds exactly:

- `vulkan-compute.cpp:145` — `setComputeState` ends with `commitBarriers();`
- `vulkan-compute.cpp:166-173` — `dispatch` is `assert` +
  `updateComputeVolatileBuffers()` + `cmdBuf.dispatch()`. **No `commitBarriers`.**

Read the whole function, not a grep window: `:166-173` is the complete body.
There is no barrier work in it.

This is a genuine refutation, not a rewording. Card D's stated mechanism ("its
*execution* forces nvrhi to emit the pending layout transitions") is false; the
emission is at `setComputeState`, which is one call earlier and is present in
every consuming pass regardless.

## Plan completeness

I checked the plan's two supporting claims and one number:

1. **Generation also flushes before temporal's binding sets exist.**
   `FReSTIRPass.cpp:400` `CmdList->setComputeState(ComputeState)` in
   `DispatchGeneration`, versus temporal's binding-set creation at `:481`/`:489`.
   Ordering claim holds.
2. **`:1169`'s manual `commitBarriers()` is not a precedent for the card.**
   Confirmed — its comment (`:1163-1168`) describes the intra-`setComputeState`
   hazard (descriptors bound before pending barriers land), and it precedes
   `ReBLURPass.Dispatch` whose binding set is created *inside* that call. The
   planner's asymmetry argument is correct and is the strongest part of the plan.
3. **`AccumInput` assignment count.** Plan says two, at `:1111` and `:1180`.
   Re-ran `search_files pattern="AccumInput"` → 4 hits: `:1123` (declaration +
   ternary), `:1180` (reassignment), `:1190`, `:1203` (uses). **The plan's
   `:1111` is off by twelve — the correct site is `:1123`.** The *claim*
   (exactly two assignments, second one post-ReBLUR-overwrite) is correct and
   the deadness argument is unaffected, but the citation is wrong.

   This is the third consecutive cycle in which a line-number cross-reference
   was found stale at a later gate. It does not warrant FIX — the plan
   recommends no code change, so no wrong number can reach the source — but the
   impler must not propagate `:1111` into the comment it writes. Recorded as a
   binding constraint below.

## Why KEEP and not FIX

The plan reaches the unusual conclusion "make no functional change," and my
instinct at this gate is to treat that as evasion — this lineage has 500+ ticks
of no-change closures behind it, and the anti-pattern is well established.

It is not evasion here, for a checkable reason: **the plan produces a
falsification that changes what the next action should be.** Prior no-change
ticks re-verified that things were fine. This one shows a queued instruction was
wrong, and would have committed a placebo line plus a real deletion into the
source on a false premise. Declining to implement a bad card is the correct
output of a plan gate, not a skipped cycle.

The plan also does not stop at "no": it converts card D into a one-run operator
experiment with a binary outcome, which is strictly more actionable than the
card was.

## Constraint on the impler (binding)

- **Comment text only.** Any functional edit to `:845-870` is an automatic FIX.
- **No numeric line-number cross-references in the new comment** — symbolic names
  only. The one permitted exception is the external nvrhi file reference
  (`vulkan-compute.cpp`), which is stable and is the whole point of the citation.
  Do *not* carry `:1111` forward.
- Do not delete the dispatch this cycle. Option B is operator-gated on
  absence-evidence, correctly.

## Feedback for planner (FIX only)

None — KEEP.
