# Pending Plan v199

- task: Card L's two deferred sibling checks — apply v198's set-difference procedure to
  `TestRTReflections.cpp` and `TestRenderSponza.cpp`
- source: no bundle — direct source read
- approach: Card L (the only actionable `- [ ]` in PENDING_PICK) has an explicit
  precondition — **do not action the `TestCornellBoxGI.cpp` remedy while the v183-v198
  chain is unbuilt** — and this tick cannot build (terminal categorically denied, see
  `risks`). But the card carries a **second, separable instruction that is NOT
  build-gated**: *"Two further targets need the same check, and with the SET-DIFFERENCE
  query shape rather than an extent-source sweep — `TestRTReflections.cpp:892-899` and
  `TestRenderSponza.cpp:413-416`."* That half is a pure determination, source-decidable,
  and touches nothing. This cycle does that half and only that half.
- diff_estimate: +0 / -0 lines of source. Determination-only, like v196 and v198.
- skip_plan_review: no
- test_strategy: file-only verifier. For each target, the tester must **re-derive the
  partition by hand** rather than by query, per the checklist row v198 added: enumerate
  every `createTexture` / `createStagingTexture` in the file, read the resize-block
  bounds, and partition the creation sites against those bounds by reading line numbers,
  not by grepping for an absence.
- risks:
  - **The card's own description may be wrong.** Standing rule from cards E/G/H/J, at
    full strength: *a card's description of code is evidence about the card's author, not
    about the code.* Card L asserts these two targets "have the same resize-block shape."
    That must be re-derived, and if the shape differs the finding is that the card was
    wrong, not that the check was inapplicable.
  - **Confirmation gradient toward an eleventh instance.** Ten cards have found ten
    instances. The pull is to find a defect because the sweep has always found one. If
    both targets are clean, the honest output is "clean" — pre-committing here, before
    the read, exactly as v198 pre-committed its no-patch. If either is clean, say clean
    and do not reach for a weaker instance to justify the cycle.
  - **`TestRenderSponza.cpp` may be out of scope entirely.** Card L cites `:413-416` as a
    resize block, but if that target has no compute/RT dispatch and no extent-sized
    resources, the set difference is vacuously empty and the correct finding is that the
    card mis-targeted it. That is a real outcome, not a failure to check.
  - Terminal is denied categorically, so nothing here can be built or run. This cycle
    must not claim otherwise, and must not carry forward the 2026-08-14 log as evidence
    about the current tree (that log predates v183).
