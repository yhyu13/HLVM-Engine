# Pending Plan v197

- task: Card K — `RenderGBuffer` advertises an extent contract its body no longer honours
- source: no bundle — direct edit
- approach: Drop both parameters from `RenderGBuffer(uint32_t /*W*/, uint32_t /*H*/)`
  and drop the two arguments at its single call site `RenderGBuffer(FB.width, FB.height)`.
  This is a **signature change, not a substitution**. Substituting `WIDTH`/`HEIGHT`
  into parameters that are already commented out would preserve exactly the
  misleading appearance the card is about — the call site would still read as
  though the raster extent is a caller-supplied quantity, and the next reader
  would still have to open the callee to learn it is discarded. Removing the
  parameters makes the fact structural: the pass renders into a fixed-extent
  target, so there is no extent for a caller to supply.
- diff_estimate: +12 / -3 (3 functional lines changed, ~12 comment lines added)
- skip_plan_review: no

## REVISION v197.2 — scope added at the plan gate (FIX → re-planned)

`PENDING_PLAN_REVIEW_v197.md` returned **FIX** with one substantive finding, now
absorbed. **Third functional site, inside the same function:** `:2418-2419` logs
`"RenderGBuffer frame {}: drew {} meshes, viewport {}x{}", FrameCount, MeshCount,
LastWidth, LastHeight` — but the viewport this function sets is `:2353`
`nvrhi::Viewport Vp(0.f, float(WIDTH), 0.f, float(HEIGHT), ...)`. v195 substituted
the viewport and left the log reporting the pre-v195 quantity. Both read 800x600
today, so the line is accidentally truthful; under the resize v195's fix exists to
survive, **the log would report the swapchain extent while the pass rasterises at
`WIDTH`/`HEIGHT`** — a diagnostic that lies exactly when consulted, in a lineage
whose entire evidence chain is log lines of this kind. Fix: `LastWidth, LastHeight`
→ `WIDTH, HEIGHT`.

Bundling accepted with the gate's argument: this cycle's enumeration is over
`RenderGBuffer` **call shapes**, and a log format string is not a call shape, so
the two candidate sets do not interact and both stay independently verifiable.

`LastWidth`/`LastHeight` the *variables* remain untouched — they are resize
detection state at `:754`, and v195 recorded why substituting the variable rather
than a use of it silently disables `BindingCache.Clear()`.

- test_strategy: file-only verification with **stated before/after polarity for
  every query** (the gate's FIX 2). The tester must confirm:
  (a) definition parameter list empty — `void RenderGBuffer(uint32_t` 1→0;
  (b) call site argument list empty — `RenderGBuffer(FB.width, FB.height)` 1→0;
  (c) the new shape appears at both sites — `RenderGBuffer()` **0→2**, which is
      the controlled zero that proves the query is well-formed rather than merely
      matching nothing;
  (d) no second call site or forward declaration — `RenderGBuffer(` 2→2, both
      being the definition and the one call;
  (e) the log reports the viewport quantity — `MeshCount, WIDTH, HEIGHT` 0→1;
  (f) `FB.width` survives **only** at its three deliberate sites (resize detect,
      `LastWidth` assign, blit destination) plus comment prose.
  Every zero paired with a same-shape positive per the tick-526 / v192 / v196
  false-zero rules; no `|` alternation; no query pasted from a line that wraps.
- risks:
  1. **A missed call site or forward declaration is a compile error, not a silent
     defect.** Verified pre-plan: `void RenderGBuffer` → 1 hit (the definition),
     `RenderGBuffer(FB.width, FB.height)` → 1 hit, `RenderGBuffer(WIDTH, HEIGHT)`
     → 0, `RenderGBuffer()` → 0. The remaining `RenderGBuffer` hits are comment
     prose and log strings. The impler must re-derive this set, not inherit it.
  2. **The v183-v197 chain has never been built.** This cycle touches the
     acceptance-path file. A compile error introduced here would surface at that
     chain's first build alongside fourteen other unbuilt cycles. Mitigation: the
     edit is two lines with a mechanically checkable candidate set, and unlike a
     substitution its failure mode is loud (the compiler rejects an arity
     mismatch) rather than silent.
  3. **This is not a defect today**, so the bar for touching the file at all must
     be argued, not assumed. The argument: card K's whole content is that the
     *appearance* is wrong, and an appearance defect can only be fixed by
     changing the appearance. Deferring it indefinitely leaves a trap that no
     query shape flags — a `FB.width` sweep sees a hit that looks correct, and a
     signature sweep sees unused parameters that look inert. It is precisely the
     kind of thing that is cheap now and expensive after someone un-comments a
     parameter.

## Question for the plan-criticer

v196 declined to patch on the ground that the v183-v196 chain is unbuilt and the
file was the known-good control. **That reasoning does not transfer** — this file
is the target, not the control, and it already carries fourteen unbuilt cycles of
changes. But the gate should rule explicitly on whether "unbuilt chain" is a
reason to stop making source changes in the *target* too. If it is, this cycle
should close as a no-patch determination like v196; if it is not, say so, because
the same question will be asked of every remaining card.
