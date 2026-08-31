# Pending Plan Review v203

- plan: docs/PENDING_PLAN_v203.md
- verdict: KEEP (with one correction and one premise the plan did not state)
- reviewer: agent_2_plan_criticer (tick-549)
- timestamp: 2026-08-30

## Design soundness

The plan is sound and is NOT re-litigating v202. Verified the distinction
directly: v202's marker and its three comment blocks touch
`GenerationLayoutSRV`, `DispatchGeneration`'s binding set, and `SpatialLayout`'s
SRV/UAV *split* question. **It never checked any layout's binding LIST against
the temporal shaders.** `FReSTIRPass` owns five layouts; v202 applied its own
new invariant to one. Sweeping the rest is the completion of v202, not a repeat.

This is the recurring shape in this lineage and it is worth naming: **a cycle
discovers a general invariant on the instance that provoked it and then closes
without sweeping the invariant's own domain.** v198 did this too (found the
set-difference procedure, applied it to one file; v199 and v201 spent two
further cycles sweeping it). Same pattern here, one cycle later.

## Correction to the plan

The plan says "sweep `TemporalLayout{SRV,UAV}` and `SpatialLayout`". That
enumeration is incomplete for its own stated purpose: the invariant ranges over
**layout-vs-consumer pairs**, and there are two consumers, so the sweep is six
pairs, not three. Verified the second consumer is real rather than nominal —
`TestCornellBoxGI.cpp:1612` calls `DispatchTemporal`, guarded by
`CVar_r_ReSTIR_EnableTemporal` (`:1610`), and `:1653` sets `SpatDesc.OutRadiance`
so spatial runs too. The control is a live consumer of all three layouts.

## Premise the plan did not state, and which changes the severity

The plan treats this as an audit that will most likely find nothing. It will
not. I checked one pair before endorsing, because endorsing a sweep whose first
pair is already broken would be rubber-stamping:

- `TemporalLayoutSRV` declares cbuffer + **t0..t9** (`FReSTIRPass.cpp:221-233`).
- The primary's `ReSTIR_Temporal_cs.hlsl` declares t0..t9 — **matches**.
- The control's copy declares **t0..t7 only** (`:48-55`); `gCurrRadiance`/
  `gHistRadiance` are absent.
- `TemporalLayoutUAV` declares **three** UAVs at 384/385/386 (`:246-250`).
- The control's copy declares **two** UAVs, `u0`/`u1`, **and in the default
  register space** (`:57-58`), where the primary uses `u0/u1/u2, space1`
  (`:61-63`).

So this is materially worse than card M. Card M was one missing SRV kept
harmless by a ternary fallback. Here the control's temporal shader diverges in
**binding count on both sets AND in descriptor set membership** — `space1` vs
default space decides whether the UAVs land in SPIR-V set 1 or set 0, and the
C++ side composes the pipeline from two layouts on the assumption of set 0 / set
1 (`:207-211` states exactly that reflection). A `space1` sweep is the controlled
positive: the control's generate copy **does** use `space1` (`:36-37`), so its
temporal copy's omission is a divergence within the same file pair, not a
project-wide convention difference.

## Plan completeness

Missing from the plan, required before the impler writes anything:

1. Enumerate all six pairs explicitly; do not stop at the first defect.
2. For every zero, a same-shape positive control in the same file or directory
   (tick-526's alternation rule; v199's `path`-at-a-directory rule).
3. Check whether the control has ever actually run this path — a log at
   `Binary/Debug/TestCornellBoxGI.log` exists and is the only runtime evidence
   available to this runspace. Report what it shows, including if it shows
   nothing conclusive.

## Answer to the plan's question 2

**Card L's precondition still holds, and a broken control strengthens it rather
than weakening it.** The plan asked whether discovering the control is itself
defective changes the argument for leaving it unmodified. It does not, for a
reason worth recording: the control's value is that its *provenance* is
unmodified, so that when the v183-v202 chain is first built, any failure in the
control is attributable to the chain rather than to us. A control with a known,
documented defect is still a usable control — the defect is a known constant. A
control we have edited is not, because a failure could then be ours. **Document
every divergence precisely enough that the operator can fix it in one pass after
the first build; change nothing.**

## Feedback for planner (FIX only)

None — KEEP with the corrections above folded in.
