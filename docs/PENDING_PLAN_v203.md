# Pending Plan v203

- task: Apply v202's NEW invariant (layout-vs-each-consumer) to the two shared
  layouts v202 did NOT check — temporal and spatial — since v202 only checked
  the generation layout it happened to be looking at.
- source: no bundle — direct source analysis
- approach: v202 discovered the invariant *for a shared binding layout, every
  consumer's shader must declare every binding the layout declares* and then
  applied it to exactly one of the three layouts `FReSTIRPass` owns. An
  invariant discovered on one instance and never swept across its own domain is
  half-done work. Sweep `TemporalLayout{SRV,UAV}` and `SpatialLayout` against
  BOTH consumers' shader copies.
- diff_estimate: +0 functional / comment-only, one shared file
- skip_plan_review: no
- test_strategy: file-only queries with same-shape positive controls for every
  zero; no count quoted from another marker.
- risks:
  - The natural wrong move is to "fix" the control's shader by adding the
    missing registers. Card L's precondition (do not modify the known-good
    control while the v183-v202 chain is unbuilt) governs.
  - v202's own framing risk repeats: a question phrased "does X need Y?"
    invites adding Y. Phrase findings as verdicts, not as invitations.

## Questions for the plan gate

1. Is the sweep sound, or is it re-litigating v202? (v202 checked ONE layout.)
2. If a divergence is found in the control, does card L's precondition still
   hold, or does a *broken* control change the argument for leaving it alone?
