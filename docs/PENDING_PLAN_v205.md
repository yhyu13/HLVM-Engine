# Pending Plan v205

- task: card O — the cross-operand guide-extent invariant introduced by v204's own fix
- source: no bundle — direct edit
- approach: Card O says `FBilateralDenoisePass::Dispatch` derives `GuideScale`
  from the normal guide and applies it to both guides, and asks for a real
  choice: document the invariant at the `FDesc` declaration, or add a
  per-guide scale. Before choosing, close the assignment set of every guide
  extent in both consumers (the v204 rule), and re-read the callee rather
  than trusting the card's description of it (the v195 standing rule, after
  cards E/G/H each dissolved on reading).
- diff_estimate: +10 / -2
- skip_plan_review: no
- test_strategy: file-only structural verification; every zero controlled by a
  same-shape positive; no `|` alternation (tick-526); read every returned diff
  (v203)
- risks: (1) the card's own framing may be wrong — three cards in this lineage
  described a callee incorrectly; (2) the header is included by both consumers,
  so a real signature change perturbs the known-good control's compile path
  while the v183-v204 chain is still unbuilt; (3) a "no-op for the control"
  claim must close assignment sets, not compare names (v204 row 17).

## The question the plan gate must rule on

Card O frames this as a choice between documenting and parameterising. **Do not
accept that framing without reading the callee.** Specifically: is
`NormalTexture` — the operand the scale is derived from — actually guaranteed
present? The header comments it "(optional)". If it is genuinely optional, the
card has described the wrong defect: the problem would not be that the two
guides *might* differ, but that the scale is sourced from the guide that is
allowed to be absent, in which case the fix is neither of the card's options.
