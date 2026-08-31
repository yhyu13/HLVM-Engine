# Pending Plan Review v197

- plan: docs/PENDING_PLAN_v197.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-543)
- timestamp: 2026-08-30

## Re-review after v197.2 (FIX → KEEP)

Both FIX items absorbed. The revised plan carries the third site with the
bundling argument stated rather than assumed, and every verification query now
declares its before/after polarity — including the controlled zero
(`RenderGBuffer()` 0→2) that distinguishes a landed patch from a malformed query.
`LastWidth`/`LastHeight` correctly left as resize-detection state. **KEEP.**

Original review follows.

---

## Design soundness

The remedy is right and the reasoning for *why it must be a signature change
rather than a substitution* is the strongest part of the plan: an appearance
defect can only be fixed by changing the appearance, and substituting
`WIDTH`/`HEIGHT` into already-commented-out parameters would leave the call site
reading exactly as misleadingly as it does now. I re-derived the candidate set
independently rather than accepting it: `RenderGBuffer(` across the whole
`Engine/Source/Runtime` tree → **2 hits**, the definition at `:2162` and the
single call at `:793`. Widening to bare `RenderGBuffer` across `Engine/` → 20
hits, every additional one classified as comment prose, a log format string, a
recipe echo, or a handoff doc. **There is no forward declaration and no second
call site**, so the arity change cannot break a caller I have not seen. The class
is a single `struct`-scoped method; no virtual, no override, no function pointer.

## Ruling on the plan's explicit question

The plan asked the gate to rule on whether v196's "the chain is unbuilt, so do
not touch source" reasoning transfers to this cycle. **It does not, and the plan
is right about why, but for an incomplete reason.**

v196's argument was specifically about *the known-good control*: perturbing
`TestPathTraceGI.cpp` would correlate the instrument with the measurement,
because that file's entire diagnostic value is its provenance as unmodified.
That property is unique to the control. `TestReSTIR_GI_Temporal.cpp` is the
target and already carries the v183-v196 changes; one more edit does not change
its epistemic status.

**The reason the plan missed:** an arity mismatch is a *compile* error, and the
v183-v197 chain's first build is exactly where compile errors surface. Unlike
every substitution in this lineage — which compiles whether right or wrong, and
therefore banks its risk in silent runtime behaviour — this edit's failure mode
is caught by the compiler on the same run that the chain is first built. It is
the **safest** kind of change to make while unbuilt, not the riskiest. The plan
argued from "the control property does not apply here"; the stronger argument is
"this change's failure mode is loud."

## Plan completeness

Two gaps, one of which is a real finding.

**FIX 1 — the plan's candidate set omits a site that is genuinely wrong, and it
is inside the function being edited.** `:2418-2419` logs

    "RenderGBuffer frame {}: drew {} meshes, viewport {}x{}", FrameCount, MeshCount, LastWidth, LastHeight

but the viewport this function actually sets is `:2353`
`nvrhi::Viewport Vp(0.f, float(WIDTH), 0.f, float(HEIGHT), 0.f, 1.f)` — v195
substituted the viewport and **left the log reporting the old quantity**. Today
both read 800x600 so the line is accidentally truthful, but under the exact
resize that v195's fix exists to survive, this log would report the swapchain
extent while the pass rasterises at `WIDTH`/`HEIGHT`. That is worse than an
unused parameter: it is a **diagnostic that lies precisely when it is consulted**,
and this lineage's evidence chain is built almost entirely on log lines of this
kind. `viewport {}x{}` → 1 hit, `nvrhi::Viewport Vp` → 1 hit, so the pairing is
unambiguous and the fix is `LastWidth, LastHeight` → `WIDTH, HEIGHT`.

Include it. It is the same defect class as card K (a stale surface left by v195's
own fix, inert today, misleading on resize), it is inside the same function, and
carding it separately would be the pipeline manufacturing an eleventh card out of
a two-token change. **This is a bundling exception, argued rather than assumed:**
bundling is normally refused here because it makes a cycle's candidate-set
enumeration unverifiable — but this cycle's enumeration is over `RenderGBuffer`
call shapes, and the log line is not a call shape, so the two sets do not
interact and both remain independently checkable.

**FIX 2 — the test strategy must add a controlled zero for the arity change.**
`RenderGBuffer()` currently returns 0 hits, and after the patch it must return 2.
A zero before and a non-zero after is the only shape that distinguishes "the
patch landed" from "the query is malformed" — the failure mode that has now bitten
this pipeline four separate ways (tick-526 alternation, v192 escaped
metacharacters, v196 unescaped metacharacters, v196 line wrap). The plan's
strategy lists the queries but does not require the before/after polarity.

## Feedback for planner (FIX)

- Add `:2418-2419` — `LastWidth, LastHeight` → `WIDTH, HEIGHT` in the log call —
  to the scope, with a comment stating that the logged extent must track the
  viewport at `:2353` and not the swapchain.
- State the expected before/after polarity for each verification query, not just
  the query text.
- Leave `LastWidth`/`LastHeight` themselves alone. They are resize-detection
  state (`:754`), and v195 already recorded why substituting the variable rather
  than a use of it breaks `BindingCache.Clear()`. This cycle touches one *use*,
  not the variable.
