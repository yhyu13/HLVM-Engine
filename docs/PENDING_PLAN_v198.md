# Pending Plan v198

- task: Card I (remaining half) — sweep the sibling targets for the extent class using query shapes other than `FB.width`
- source: no bundle — direct source analysis
- approach: v197 closed card I *for the primary target* (`TestReSTIR_GI_Temporal.cpp`'s live `FB.width` set is now exactly three intentional sites). The half that remains is what v195 proved: **the class is not co-extensive with the `FB.width` query shape.** This cycle sweeps the sibling RT/GI targets under three additional query shapes — (a) `CurrentFBInfo` (the siblings' name for the same swapchain quantity), (b) `RenderTargetSize` (the cbuffer laundering route v195 found), (c) `nvrhi::Viewport` construction. Expected output is a determination per sibling, not necessarily a patch.
- diff_estimate: +0 / -0 functional expected; markers only
- skip_plan_review: no
- test_strategy: file-only verifier rows, each a single-term `search_files` with a same-shape positive control (tick-526 alternation rule; v197 assumption-encoding rule — widen before believing a zero)
- risks:
  - **The likeliest sibling hit is a known-good control.** `software-development-practices §Path-Tracing / RT Debugging Methodology` rule 4 names `TestCornellBoxGI` as the working control that exonerates the driver, nvrhi, slangc and the binding layer. v196 established that the v183-v197 chain has **never been built**, so a defect introduced into a control now would surface at that chain's first build and be indistinguishable from a real regression in the eleven cycles the control exists to exonerate. **If the sweep hits a control, the correct output is a determination and a card, not a patch.** State this in advance so it is not rationalised afterwards.
  - Card I's premise was already falsified once (v195). Do not assume the three query shapes above are exhaustive either; report the shape that found anything as *a* shape, not *the* shape.
  - Cards E, G, H each asserted something about a callee that dissolved on reading it. Read every callee before asserting.

## Findings the plan is built on (each re-derived this cycle, not carried forward)

1. Primary target `FB.width` → 16 hits, of which **3 are live code** (`:754` resize detect,
   `:756/:757` `LastWidth`/`LastHeight` assign, `:1326` blit destination) and the rest are
   comments. Enumeration clean, matching v197's claim.
2. `RenderTargetSize` → 21 hits tree-wide. The two writes outside the primary target are
   `TestPathTraceGI.cpp:857-858` (determined not-a-defect by v196) and
   `TestCornellBoxGI.cpp:1283-1284`.
3. `nvrhi::Viewport` → 20 hits. Every non-`WIDTH` construction is in a sibling that sizes its
   render targets from the same `CurrentFBInfo`, i.e. self-consistent — **except** where a
   resource is created once and not recreated on resize.
4. `CurrentFBInfo` → 80 hits across 8 sibling targets.

## The determination this cycle must make

For each sibling that dispatches ReSTIR/RT work: are the resources it dispatches over
**recreated on the same resize event** that moves the dispatch extent? A target whose
extent source is uniformly `CurrentFBInfo` is *not* automatically safe — safety requires
that every resource in the dispatch's resource set is recreated when `CurrentFBInfo`
changes. **This is a lifetime question, not an extent-source question**, and no `FB.width`
or `CurrentFBInfo` sweep can answer it, because every site in such a file reads correct.
