# Pending Impl Review v199

- plan: docs/PENDING_PLAN_v199.md
- commit: docs/PENDING_COMMIT_v199.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-545)
- timestamp: 2026-08-30

## plan_fidelity_check

The plan asked for card L's second half — the two sibling checks — and pre-committed to
reporting "clean" if clean. The impl did exactly that and reported clean. One deviation is
declared (a third file), and it is **justified**: it is a read, it modified nothing, and it
covers the class's most defect-prone shape (RT dispatch writing a UAV) which the card's two
names left unexamined. Per the deviation policy the test is whether the design changed
without planner sign-off; the design here was "apply the procedure and determine," and
applying it to one more file of the same shape does not change it. Accepted.

**The precondition question I re-tested rather than inherited.** The plan gate ruled that
card L's build-gate binds the remedy, not the checks. I re-read the card: the precondition
sentence's object is the `TestCornellBoxGI.cpp` block move, and the sibling sentence is a
separate instruction using the word "check." A cycle that modifies zero bytes cannot
perturb the control whose unmodified provenance the precondition exists to protect.
Upheld — and note this cycle's zero-byte output is what makes that argument available; had
the impler patched anything the verdict would be FIX regardless of correctness.

## Independent re-derivation of the load-bearing rows

I did not accept the impler's tables. Three rows could invert the verdict:

**Row 1 — target 1's resize block bounds.** If the block closed earlier than `:984`, the
later creations would fall outside it and the file would be defective. Read `:892` (`if
(!GBufferNormalsTexture || CurrentFBInfo.width != LastWidth ...)`) and `:983-984`
(`BindingCache.Clear();` / `}`). Every creation the impler listed at `:912`-`:980` lies
inside. **Confirmed.**

**Row 2 — the framebuffer, which the impler listed but did not enumerate.**
`createFramebuffer` → 2 hits, `:513` init and `:940` resize. A recreated set of MRT
textures behind a *stale* framebuffer would be a real defect of a shape none of the ten
prior instances had, and the impler's table asserted containment without showing the
count. It holds — 2 hits, one per block. **Confirmed, but the impler should have shown
this count rather than asserting the row.**

**Row 3 — the deviation target.** `TestRTShadowsGBuffer.cpp` resize block opens `:781`,
closes `:872`; creations `:801`-`:869` inside; `createFramebuffer` → 2 hits (`:401`,
`:829`), one per block; dispatch at `args.width = CurrentFBInfo.width` (`:1032`). Same
one-extent-source-throughout property. **Confirmed clean.**

## The finding I want on record, because it is worth more than the three clean verdicts

The impler framed target 1 as a **positive control for the set-difference procedure**, and
that framing is correct and load-bearing. v198 introduced the procedure and immediately
found a defect with it. A detection method that has only ever been exercised where it fires
has demonstrated sensitivity and **not** specificity — it could have been flagging every
file with a resize block. Target 1 is superficially the *same shape* as the defective file
(resize block, extent-sized UAVs, RT dispatch, `Resizable = true`) and comes back clean on
a ten-for-ten containment. That is the first evidence the procedure discriminates rather
than merely fires, and it should be cited whenever the procedure is next used.

**Corollary that sharpens card L rather than softening it.** Three of four files with this
shape are clean; the defective one is the outlier. So `TestCornellBoxGI`'s defect is not
"the codebase-wide pattern" — it is a genuine local omission, which *raises* confidence in
card L's diagnosis while lowering the expected yield of further sweeping.

## Security scan

- [x] No hardcoded secrets — no source modified
- [x] No shell injection — no shell invoked (denied)
- [x] No eval/exec — N/A
- [x] No SQL injection — N/A

## Self-review checklist

- [x] Validation: three files enumerated completely; bounds read, not grepped
- [x] Error handling: N/A (no code changed)
- [x] Tests: file-only verifier; no test files produced

## What this cycle did NOT establish

That anything builds, runs, renders or validates. **0/7 acceptance gates.** The v183-v199
chain remains uncompiled, which is now the single blocking fact for this lineage and is not
addressable from this runspace.

## Feedback for impler (FIX only)

None. KEEP. The one criticism (asserting the framebuffer row instead of showing its count)
did not change the outcome and is recorded above rather than as a FIX.
