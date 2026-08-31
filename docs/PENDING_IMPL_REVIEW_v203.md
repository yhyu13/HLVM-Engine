# Pending Impl Review v203

- plan: docs/PENDING_PLAN_v203.md
- commit: docs/PENDING_COMMIT_v203.md
- verdict: KEEP (with one line-number correction and one severity re-grade)
- reviewer: agent_4_reviewer (tick-549)
- timestamp: 2026-08-30

## plan_fidelity_check

Two deviations declared, both justified. The first (three layouts → six pairs)
is the plan gate's own correction, so following it over the plan is correct per
`§Impler deviation policy`. The second (found defects, fixed none) is governed
by card L's precondition and was ruled on at the plan gate before the impler
acted, not rationalised afterwards.

## Correction — stale line numbers in the marker

The impler cites `:562-563` for "slots 8 and 9 bound unconditionally". Re-queried:
`:562-563` are `Texture_SRV(2, HistoryReservoir0)` / `Texture_SRV(3,
HistoryReservoir1)`. The actual slot-8/9 bindings are at **`:568-569`**. The
*claim* is correct — I verified both items are present and unconditional — but
the citation is off by six, drifted by this cycle's own insertions.

This is the third cycle in four where an impler quoted a line number invalidated
by its own edits (v192 caught its own, v202 logged the LSP variant). **The
standing lesson stands and is being re-learned rather than applied: re-query
every `:NNNN` after the last edit, not before.**

## Severity re-grade — the impler UNDER-graded its own finding

The marker frames card N as "worse than card M because there is no fallback."
True but incomplete. The UAV divergence is worse than that framing conveys, and
the reason is visible in the file the impler was already reading:

`DispatchTemporal` creates its UAV binding set against `TemporalLayoutUAV` with
**three** items at 384/385/386 (`:575-580`), and `TestCornellBoxGI.cpp:1607`
supplies `OutRadiance`. The control's shader declares **two** UAVs. So the
control is not merely missing a receiver — the C++ side and the shader disagree
on the **arity of a descriptor set** *and* on **which set it is**. Card M's
mismatch was one SRV slot inside an otherwise-agreeing set; this is a
set-shaped disagreement, which is the kind nvrhi validates at pipeline creation
rather than tolerating.

I am deliberately NOT claiming what the runtime does with that — I cannot run
it. The re-grade is about the *shape* of the disagreement, which is
source-decidable, not about a predicted failure.

## The near-miss is the most valuable thing in this cycle

The impler self-reported that its third `patch` deleted `ConstantBuffer(256)`,
`Texture_SRV(0)` and `Texture_SRV(1)` from `SpatialLayout`, and restored them.
I verified the restoration independently rather than accepting it: `:325-333`
holds 7 items in the correct order, `createBindingLayout` → 5, `LayoutDesc.bindings`
→ 5, and the spatial list matches the shader's t0..t4 + u0.

**Two things follow, and the second is the one worth carrying forward.**

1. A "comment-only, 0 functional lines" claim is **not self-certifying**. This
   cycle would have shipped a broken layout under that exact claim if the diff
   had gone unread. v202 asserted comment-only diffs have one realistic failure
   mode; v203 demonstrates it empirically, one cycle later.
2. The mechanism is specific and preventable: **an `old_string` anchored on a
   comment that sits directly above a braced initialiser will match into the
   initialiser.** Anchor on the statement boundary. This is a tooling rule, not
   a diligence exhortation, and it belongs in the checklist as such.

## Security scan

- [x] No hardcoded secrets — comment-only
- [x] No shell injection — N/A
- [x] No eval/exec — N/A
- [x] No SQL injection — N/A

## Self-review checklist

- [x] Validation: all five layout lists verified item-by-item post-edit
- [x] Error handling: no control flow touched
- [x] Tests: file-only; every zero has a same-shape positive control
- [x] Known-good control byte-unchanged; no `.hlsl` touched
- [x] No commit, no push, no governance file

## Card opened at this gate

**Card N** — the control's `ReSTIR_Temporal_cs.hlsl` diverges from the shared
temporal layouts on both sets (t0..t7 vs t0..t9; two default-space UAVs vs three
in `space1`). Same build precondition as cards L and M. **Action all three
together after the chain's first build** — they are one edit pass in one
directory, and doing them separately means three separate perturbations of the
control's provenance.

## Feedback for impler (FIX only)

None blocking. Line-number correction folded in above.
